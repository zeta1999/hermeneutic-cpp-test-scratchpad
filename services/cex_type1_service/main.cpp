#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Timespan.h>
#include <Poco/Util/ServerApplication.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_running{true};

void handleSignal(int) {
  g_running = false;
}

class FeedRequestHandler : public Poco::Net::HTTPRequestHandler {
 public:
  FeedRequestHandler(std::string exchange,
                     std::string token,
                     std::vector<std::string> payloads,
                     std::chrono::milliseconds interval)
      : exchange_(std::move(exchange)),
        token_(std::move(token)),
        payloads_(std::move(payloads)),
        interval_(interval) {}

  void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override {
    const auto auth = request.get("Authorization", "");
    if (!token_.empty() && auth != "Bearer " + token_) {
      response.setStatus(Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED);
      response.send() << "unauthorized";
      return;
    }

    try {
      Poco::Net::WebSocket ws(request, response);
      ws.setSendTimeout(Poco::Timespan(5, 0));
      ws.setReceiveTimeout(Poco::Timespan(5, 0));
      spdlog::info("{} client connected", exchange_);
      while (g_running.load()) {
        for (const auto& payload : payloads_) {
          ws.sendFrame(payload.data(), static_cast<int>(payload.size()), Poco::Net::WebSocket::FRAME_TEXT);
          std::this_thread::sleep_for(interval_);
          if (!g_running.load()) {
            break;
          }
        }
      }
    } catch (const std::exception& ex) {
      spdlog::warn("{} WebSocket error: {}", exchange_, ex.what());
    }
  }

 private:
  std::string exchange_;
  std::string token_;
  std::vector<std::string> payloads_;
  std::chrono::milliseconds interval_;
};

class FeedRequestFactory : public Poco::Net::HTTPRequestHandlerFactory {
 public:
  FeedRequestFactory(std::string exchange,
                     std::string token,
                     std::vector<std::string> payloads,
                     std::chrono::milliseconds interval)
      : exchange_(std::move(exchange)),
        token_(std::move(token)),
        payloads_(std::move(payloads)),
        interval_(interval) {}

  Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest& request) override {
    const auto expected_path = "/" + exchange_;
    if (request.getURI() != expected_path) {
      return nullptr;
    }
    return new FeedRequestHandler(exchange_, token_, payloads_, interval_);
  }

 private:
  std::string exchange_;
  std::string token_;
  std::vector<std::string> payloads_;
  std::chrono::milliseconds interval_;
};

std::vector<std::string> loadPayloads(const std::string& path) {
  std::ifstream stream(path);
  if (!stream.is_open()) {
    throw std::runtime_error("could not open feed file: " + path);
  }
  std::vector<std::string> payloads;
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      payloads.push_back(line);
    }
  }
  if (payloads.empty()) {
    throw std::runtime_error("feed file has no payloads: " + path);
  }
  return payloads;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) {
    spdlog::error("Usage: cex_type1_service <exchange> <file> <port> <token> [interval_ms]");
    return 1;
  }

  const std::string exchange = argv[1];
  const std::string file = argv[2];
  const unsigned short port = static_cast<unsigned short>(std::stoi(argv[3]));
  const std::string token = argv[4];
  int interval_ms = 200;
  if (argc > 5) {
    interval_ms = std::stoi(argv[5]);
  }

  try {
    auto payloads = loadPayloads(file);
    Poco::Net::ServerSocket socket(port);
    Poco::Net::HTTPServerParams::Ptr params = new Poco::Net::HTTPServerParams;
    params->setMaxQueued(10);
    params->setMaxThreads(1);
    Poco::Net::HTTPServer server(new FeedRequestFactory(exchange, token, payloads,
                                                        std::chrono::milliseconds(interval_ms)),
                                 socket,
                                 params);

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    server.start();
    spdlog::info("{} feed serving {} on ws://localhost:{}/{}", exchange, file, port, exchange);

    while (g_running.load()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    server.stop();
  } catch (const std::exception& ex) {
    spdlog::error("cex_type1_service failed: {}", ex.what());
    return 1;
  }
  return 0;
}
