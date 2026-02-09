#pragma once

#include <grpcpp/grpcpp.h>

#include <string>

#include "aggregator.grpc.pb.h"
#include "hermeneutic/aggregator/aggregator.hpp"

namespace hermeneutic::aggregator {

class AggregatorGrpcService final : public hermeneutic::grpc::AggregatorService::Service {
 public:
  AggregatorGrpcService(AggregationEngine& engine,
                        std::string token,
                        std::string symbol);
  ~AggregatorGrpcService() override = default;

  ::grpc::Status StreamBooks(::grpc::ServerContext* context,
                             const hermeneutic::grpc::SubscribeRequest* request,
                             ::grpc::ServerWriter<hermeneutic::grpc::AggregatedBook>* writer) override;

 private:
  bool authorize(const ::grpc::ServerContext& context) const;

  AggregationEngine& engine_;
  std::string expected_token_;
  std::string symbol_;
};

}  // namespace hermeneutic::aggregator
