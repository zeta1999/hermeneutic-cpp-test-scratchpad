#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Timespan.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

#include "hermeneutic/cex_type1/feed.hpp"

namespace {
class LoopbackHandler : public Poco::Net::HTTPRequestHandler {
 public:
  LoopbackHandler(std::string token, std::string payload)
      : token_(std::move(token)), payload_(std::move(payload)) {}

  void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
    try {
      const auto auth = request.get("Authorization", "");
      if (!token_.empty() && auth != "Bearer " + token_) {
        response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
        response.send() << "unauthorized";
        return;
      }

      Poco::Net::WebSocket ws(request, response);
      ws.setSendTimeout(Poco::Timespan(1, 0));
      ws.sendFrame(payload_.data(), static_cast<int>(payload_.size()), Poco::Net::WebSocket::FRAME_TEXT);
    } catch (const Poco::Exception&) {
      // Ignore transport errors triggered when the client disconnects early.
    }
  }

 private:
  std::string token_;
  std::string payload_;
};

class LoopbackFactory : public Poco::Net::HTTPRequestHandlerFactory {
 public:
  LoopbackFactory(std::string exchange, std::string token, std::string payload)
      : exchange_(std::move(exchange)), token_(std::move(token)), payload_(std::move(payload)) {}

  Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override {
    const auto expected = "/" + exchange_;
    if (request.getURI() != expected) {
      return nullptr;
    }
    return new LoopbackHandler(token_, payload_);
  }

 private:
  std::string exchange_;
  std::string token_;
  std::string payload_;
};

}  // namespace

TEST_CASE("cex_type1/websocket_loopback feed emits updates") {
  using namespace std::chrono_literals;
  const std::string exchange = "loop";
  const std::string token = "unit-token";
  const std::string payload =
      R"({"type":"new_order","sequence":1,"order_id":"ord-1","side":"bid","price":"101.25","quantity":"0.5"})";
  std::signal(SIGPIPE, SIG_IGN);

  try {
    std::unique_ptr<Poco::Net::ServerSocket> socket;
    try {
      socket = std::make_unique<Poco::Net::ServerSocket>(Poco::Net::SocketAddress("127.0.0.1", 0));
    } catch (const Poco::Exception& ex) {
      std::cerr << "Skipping loopback test: " << ex.displayText() << std::endl;
      return;
    }
    const auto port = socket->address().port();
    Poco::Net::HTTPServerParams::Ptr params = new Poco::Net::HTTPServerParams;
    params->setMaxThreads(1);
    params->setMaxQueued(1);

    auto factory = new LoopbackFactory(exchange, token, payload);
    Poco::Net::HTTPServer server(factory, *socket, params);

    server.start();

    std::mutex mutex;
    std::condition_variable cv;
    bool received = false;
    hermeneutic::common::BookEvent captured;

    auto feed = hermeneutic::cex_type1::makeWebSocketFeed(
        {.exchange = exchange,
         .url = "ws://127.0.0.1:" + std::to_string(port) + "/" + exchange,
         .auth_token = token,
         .interval = 10ms},
        [&](hermeneutic::common::BookEvent update) {
          std::lock_guard<std::mutex> lock(mutex);
          captured = std::move(update);
          received = true;
          cv.notify_one();
        });

    feed->start();
    {
      std::unique_lock<std::mutex> lock(mutex);
      CHECK(cv.wait_for(lock, 2s, [&] { return received; }));
    }
    feed->stop();
    server.stop();

    CHECK(captured.exchange == exchange);
    CHECK(captured.kind == hermeneutic::common::BookEventKind::NewOrder);
    CHECK(captured.order.side == hermeneutic::common::Side::Bid);
    CHECK(captured.order.price == hermeneutic::common::Decimal::fromString("101.25"));
    CHECK(captured.order.quantity == hermeneutic::common::Decimal::fromString("0.5"));
  } catch (const Poco::Exception& ex) {
    std::cerr << "Poco exception: " << ex.displayText() << std::endl;
    CHECK(false);
  } catch (const std::exception& ex) {
    std::cerr << "std exception: " << ex.what() << std::endl;
    CHECK(false);
  }
}
