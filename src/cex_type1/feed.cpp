#include "hermeneutic/cex_type1/feed.hpp"

#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Timespan.h>
#include <Poco/URI.h>

#include <atomic>
#include <chrono>
#include <simdjson.h>
#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>
#include <vector>

namespace hermeneutic::cex_type1 {

class WebSocketExchangeFeed : public ExchangeFeed {
 public:
  WebSocketExchangeFeed(FeedOptions options, Callback callback)
      : options_(std::move(options)), callback_(std::move(callback)) {}

  ~WebSocketExchangeFeed() override { stop(); }

  void start() override {
    if (running_.exchange(true)) {
      return;
    }
    worker_ = std::thread(&WebSocketExchangeFeed::run, this);
  }

  void stop() override {
    running_.store(false);
    if (worker_.joinable()) {
      worker_.join();
    }
  }

 private:
  void run() {
    while (running_.load()) {
      try {
        pump();
      } catch (const std::exception& ex) {
        spdlog::warn("Feed {} connection error: {}", options_.exchange, ex.what());
      }
      if (running_.load()) {
        std::this_thread::sleep_for(options_.interval);
      }
    }
  }

  void pump() {
    Poco::URI uri(options_.url);
    Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());
    std::string path = uri.getPathAndQuery();
    if (path.empty()) {
      path = "/";
    }

    Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
    if (!options_.auth_token.empty()) {
      request.set("Authorization", "Bearer " + options_.auth_token);
    }
    Poco::Net::HTTPResponse response;
    Poco::Net::WebSocket ws(session, request, response);
    ws.setReceiveTimeout(Poco::Timespan(5, 0));

    simdjson::dom::parser parser;
    std::vector<char> buffer(4096);
    while (running_.load()) {
      int flags = 0;
      int n = ws.receiveFrame(buffer.data(), static_cast<int>(buffer.size()), flags);
      if (n <= 0) {
        break;
      }
      auto op = flags & Poco::Net::WebSocket::FRAME_OP_BITMASK;
      if (op == Poco::Net::WebSocket::FRAME_OP_PING) {
        ws.sendFrame(buffer.data(), n, Poco::Net::WebSocket::FRAME_FLAG_FIN | Poco::Net::WebSocket::FRAME_OP_PONG);
        continue;
      }
      if (op != Poco::Net::WebSocket::FRAME_OP_TEXT) {
        continue;
      }
      std::string_view payload(buffer.data(), static_cast<std::size_t>(n));
      if (payload.empty()) {
        continue;
      }
      try {
        auto doc = parser.parse(payload.data(), payload.size());
        auto obj = doc.get_object();
        auto side = std::string(obj["side"].get_string().value());
        common::MarketUpdate update;
        update.exchange = options_.exchange;
        update.side = (side == "ask") ? common::Side::Ask : common::Side::Bid;
        update.price = common::Decimal::fromString(std::string(obj["price"].get_string().value()));
        update.quantity = common::Decimal::fromString(std::string(obj["quantity"].get_string().value()));
        update.timestamp = std::chrono::system_clock::now();
        callback_(std::move(update));
      } catch (const std::exception& ex) {
        spdlog::warn("Feed {} parse error: {}", options_.exchange, ex.what());
      }
    }
  }

  FeedOptions options_;
  Callback callback_;
  std::atomic<bool> running_{false};
  std::thread worker_;
};

std::unique_ptr<ExchangeFeed> makeWebSocketFeed(FeedOptions options, ExchangeFeed::Callback callback) {
  return std::make_unique<WebSocketExchangeFeed>(std::move(options), std::move(callback));
}

}  // namespace hermeneutic::cex_type1
