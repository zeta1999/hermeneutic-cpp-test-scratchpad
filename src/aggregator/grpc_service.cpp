#include "hermeneutic/aggregator/grpc_service.hpp"

#include <atomic>
#include <chrono>

#include "hermeneutic/common/concurrent_queue.hpp"
#include "services/common/grpc_helpers.hpp"

namespace hermeneutic::aggregator {

using hermeneutic::common::AggregatedBookView;
using hermeneutic::common::ConcurrentQueue;

namespace {

class SubscriptionGuard {
 public:
  SubscriptionGuard(AggregationEngine& engine, AggregationEngine::SubscriberId id)
      : engine_(engine), id_(id) {}
  ~SubscriptionGuard() { engine_.unsubscribe(id_); }

 private:
  AggregationEngine& engine_;
  AggregationEngine::SubscriberId id_;
};

}  // namespace

AggregatorGrpcService::AggregatorGrpcService(AggregationEngine& engine,
                                             std::string token,
                                             std::string symbol)
    : engine_(engine), expected_token_(std::move(token)), symbol_(std::move(symbol)) {}

::grpc::Status AggregatorGrpcService::StreamBooks(
    ::grpc::ServerContext* context,
    const hermeneutic::grpc::SubscribeRequest* request,
    ::grpc::ServerWriter<hermeneutic::grpc::AggregatedBook>* writer) {
  if (!authorize(*context)) {
    return {::grpc::StatusCode::UNAUTHENTICATED, "missing or invalid token"};
  }
  if (!request->symbol().empty() && request->symbol() != symbol_) {
    return {::grpc::StatusCode::INVALID_ARGUMENT, "unsupported symbol"};
  }

  ConcurrentQueue<AggregatedBookView> queue;
  std::atomic<bool> active{true};
  auto subscriber_id = engine_.subscribe([&](const AggregatedBookView& view) {
    if (!active.load(std::memory_order_relaxed)) {
      return;
    }
    queue.push(view);
  });
  SubscriptionGuard guard(engine_, subscriber_id);

  AggregatedBookView snapshot;
  while (!context->IsCancelled()) {
    if (!queue.wait_pop_for(snapshot, std::chrono::milliseconds(100))) {
      if (context->IsCancelled()) {
        break;
      }
      continue;
    }
    if (!writer->Write(hermeneutic::services::grpc_helpers::FromDomain(snapshot))) {
      break;
    }
  }

  active.store(false, std::memory_order_relaxed);
  queue.close();
  return ::grpc::Status::OK;
}

bool AggregatorGrpcService::authorize(const ::grpc::ServerContext& context) const {
  if (expected_token_.empty()) {
    return true;
  }
  const auto& metadata = context.client_metadata();
  auto it = metadata.find("authorization");
  if (it == metadata.end()) {
    return false;
  }
  std::string header(it->second.data(), it->second.length());
  const std::string prefix = "Bearer ";
  if (header.rfind(prefix, 0) == 0) {
    header.erase(0, prefix.size());
  }
  return header == expected_token_;
}

}  // namespace hermeneutic::aggregator
