#include "common/book_stream_client.hpp"

#include <spdlog/spdlog.h>

#include "common/grpc_helpers.hpp"

namespace hermeneutic::services {

BookStreamClient::BookStreamClient(std::string endpoint,
                                   std::string token,
                                   std::string symbol,
                                   Callback callback,
                                   std::chrono::milliseconds reconnect_delay)
    : endpoint_(std::move(endpoint)),
      token_(std::move(token)),
      symbol_(std::move(symbol)),
      callback_(std::move(callback)),
      reconnect_delay_(reconnect_delay) {
  channel_factory_ = [this]() {
    return ::grpc::CreateChannel(endpoint_, ::grpc::InsecureChannelCredentials());
  };
}

BookStreamClient::BookStreamClient(ChannelFactory channel_factory,
                                   std::string token,
                                   std::string symbol,
                                   Callback callback,
                                   std::chrono::milliseconds reconnect_delay)
    : channel_factory_(std::move(channel_factory)),
      token_(std::move(token)),
      symbol_(std::move(symbol)),
      callback_(std::move(callback)),
      reconnect_delay_(reconnect_delay) {}

BookStreamClient::~BookStreamClient() { stop(); }

void BookStreamClient::start() {
  if (running_.exchange(true)) {
    return;
  }
  worker_ = std::thread(&BookStreamClient::run, this);
}

void BookStreamClient::stop() {
  if (!running_.exchange(false)) {
    return;
  }
  cancelActiveContext();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void BookStreamClient::cancelActiveContext() {
  std::lock_guard<std::mutex> lock(context_mutex_);
  if (active_context_ != nullptr) {
    active_context_->TryCancel();
  }
}

void BookStreamClient::run() {
  while (running_.load()) {
    std::shared_ptr<::grpc::Channel> channel;
    if (channel_factory_) {
      channel = channel_factory_();
    }
    if (!channel) {
      spdlog::warn("BookStreamClient channel unavailable, retrying in {} ms", reconnect_delay_.count());
      std::this_thread::sleep_for(reconnect_delay_);
      continue;
    }
    auto stub = hermeneutic::grpc::AggregatorService::NewStub(channel);

    ::grpc::ClientContext context;
    {
      std::lock_guard<std::mutex> lock(context_mutex_);
      active_context_ = &context;
    }
    hermeneutic::services::grpc_helpers::AttachAuth(context, token_);
    hermeneutic::grpc::SubscribeRequest request;
    request.set_symbol(symbol_);

    auto reader = stub->StreamBooks(&context, request);
    hermeneutic::grpc::AggregatedBook message;
    while (running_.load() && reader->Read(&message)) {
      auto view = hermeneutic::services::grpc_helpers::ToDomain(message);
      if (callback_) {
        callback_(view);
      }
    }
    {
      std::lock_guard<std::mutex> lock(context_mutex_);
      active_context_ = nullptr;
    }
    auto status = reader->Finish();
    if (!running_.load()) {
      break;
    }
    if (!status.ok()) {
      spdlog::warn("BookStreamClient reconnect after error: {}", status.error_message());
    } else {
      spdlog::info("BookStreamClient stream closed, reconnecting");
    }
    std::this_thread::sleep_for(reconnect_delay_);
  }
}

}  // namespace hermeneutic::services
