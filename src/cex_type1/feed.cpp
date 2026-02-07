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

namespace {

common::Side parseSide(std::string_view text) {
  return text == "ask" ? common::Side::Ask : common::Side::Bid;
}

common::Decimal parseDecimal(const simdjson::dom::element& element) {
  auto str = element.get_string();
  if (str.error() == simdjson::SUCCESS) {
    return common::Decimal::fromString(std::string(str.value()));
  }
  if (auto number_value = element.get_double(); number_value.error() == simdjson::SUCCESS) {
    return common::Decimal::fromDouble(number_value.value());
  }
  throw std::runtime_error("invalid decimal payload");
}

bool parseOrderId(const simdjson::dom::element& element, std::uint64_t& id) {
  auto numeric = element.get_uint64();
  if (numeric.error() == simdjson::SUCCESS) {
    id = numeric.value();
    return true;
  }
  auto string_value = element.get_string();
  if (string_value.error() == simdjson::SUCCESS) {
    try {
      id = std::stoull(std::string(string_value.value()));
      return true;
    } catch (...) {
      return false;
    }
  }
  return false;
}

}  // namespace

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
        auto type = obj["type"].get_string();
        if (type.error() != simdjson::SUCCESS) {
          continue;
        }
        common::BookEvent event;
        event.exchange = options_.exchange;
        event.timestamp = std::chrono::system_clock::now();
        if (auto seq = obj["sequence"].get_uint64(); seq.error() == simdjson::SUCCESS) {
          event.sequence = seq.value();
        }
        auto type_string = type.value();
        if (type_string == "snapshot") {
          event.kind = common::BookEventKind::Snapshot;
          if (auto bids = obj["bids"].get_array(); bids.error() == simdjson::SUCCESS) {
            for (auto level_element : bids.value()) {
              auto level_obj = level_element.get_object();
              common::PriceLevel level;
              level.price = parseDecimal(level_obj["price"]);
              level.quantity = parseDecimal(level_obj["quantity"]);
              event.snapshot.bids.push_back(std::move(level));
            }
          }
          if (auto asks = obj["asks"].get_array(); asks.error() == simdjson::SUCCESS) {
            for (auto level_element : asks.value()) {
              auto level_obj = level_element.get_object();
              common::PriceLevel level;
              level.price = parseDecimal(level_obj["price"]);
              level.quantity = parseDecimal(level_obj["quantity"]);
              event.snapshot.asks.push_back(std::move(level));
            }
          }
          callback_(std::move(event));
        } else if (type_string == "new_order") {
          event.kind = common::BookEventKind::NewOrder;
          std::uint64_t order_id = 0;
          if (!parseOrderId(obj["order_id"], order_id)) {
            continue;
          }
          event.order.order_id = order_id;
          auto side_value = obj["side"].get_string();
          if (side_value.error() != simdjson::SUCCESS) {
            continue;
          }
          event.order.side = parseSide(side_value.value());
          event.order.price = parseDecimal(obj["price"]);
          event.order.quantity = parseDecimal(obj["quantity"]);
          callback_(std::move(event));
        } else if (type_string == "cancel_order") {
          event.kind = common::BookEventKind::CancelOrder;
          std::uint64_t order_id = 0;
          if (!parseOrderId(obj["order_id"], order_id)) {
            continue;
          }
          event.order.order_id = order_id;
          callback_(std::move(event));
        }
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
