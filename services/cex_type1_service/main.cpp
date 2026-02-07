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
#include <cctype>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_running{true};

void handleSignal(int) {
  g_running = false;
}

class PayloadCycler {
 public:
  PayloadCycler(std::string path, std::uint64_t skip_until)
      : path_(std::move(path)), skip_until_(skip_until), past_threshold_(skip_until == 0) {
    reopen();
  }

  std::string next() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (;;) {
      std::string line;
      if (!std::getline(stream_, line)) {
        reopen();
        continue;
      }
      if (line.empty()) {
        continue;
      }
      if (!past_threshold_) {
        if (!sequenceAllows(line)) {
          continue;
        }
        past_threshold_ = true;
      }
      return line;
    }
  }

 private:
  void reopen() {
    stream_.close();
    stream_.clear();
    stream_.open(path_);
    if (!stream_.is_open()) {
      throw std::runtime_error("could not open feed file: " + path_);
    }
  }

  bool sequenceAllows(const std::string& line) const {
    if (skip_until_ == 0) {
      return true;
    }
    auto sequence = extractSequence(line);
    return sequence >= skip_until_;
  }

  std::uint64_t extractSequence(const std::string& line) const {
    auto pos = line.find("\"sequence\"");
    if (pos == std::string::npos) {
      return 0;
    }
    pos = line.find(':', pos);
    if (pos == std::string::npos) {
      return 0;
    }
    ++pos;
    while (pos < line.size() && std::isspace(static_cast<unsigned char>(line[pos]))) {
      ++pos;
    }
    std::size_t start = pos;
    while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos]))) {
      ++pos;
    }
    if (start == pos) {
      return 0;
    }
    return std::strtoull(line.c_str() + start, nullptr, 10);
  }

  std::string path_;
  std::ifstream stream_;
  std::uint64_t skip_until_;
  bool past_threshold_;
  std::mutex mutex_;
};

class FeedRequestHandler : public Poco::Net::HTTPRequestHandler {
 public:
  FeedRequestHandler(std::string exchange,
                     std::string token,
                     std::shared_ptr<PayloadCycler> payloads,
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
        auto payload = payloads_->next();
        ws.sendFrame(payload.data(), static_cast<int>(payload.size()), Poco::Net::WebSocket::FRAME_TEXT);
        std::this_thread::sleep_for(interval_);
      }
    } catch (const std::exception& ex) {
      spdlog::warn("{} WebSocket error: {}", exchange_, ex.what());
    }
  }

 private:
  std::string exchange_;
  std::string token_;
  std::shared_ptr<PayloadCycler> payloads_;
  std::chrono::milliseconds interval_;
};

class FeedRequestFactory : public Poco::Net::HTTPRequestHandlerFactory {
 public:
  FeedRequestFactory(std::string exchange,
                     std::string token,
                     std::shared_ptr<PayloadCycler> payloads,
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
  std::shared_ptr<PayloadCycler> payloads_;
  std::chrono::milliseconds interval_;
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 5) {
    spdlog::error(
        "Usage: cex_type1_service <exchange> <file> <port> <token> [interval_ms] [start_sequence]");
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

  std::uint64_t start_sequence = 0;
  if (argc > 6) {
    start_sequence = std::strtoull(argv[6], nullptr, 10);
  }

  try {
    auto payloads = std::make_shared<PayloadCycler>(file, start_sequence);
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
