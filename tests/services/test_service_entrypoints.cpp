#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <Poco/Process.h>
#include <Poco/Exception.h>

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {
std::filesystem::path binaryPath(const std::string& relative) {
  return std::filesystem::path(PROJECT_BINARY_DIR) / relative;
}

std::string uniqueSuffix(const std::string& prefix) {
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return prefix + std::to_string(now);
}

std::optional<std::string> waitForCsvHeader(const std::filesystem::path& csv_path,
                                            std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    std::ifstream csv(csv_path);
    std::string header;
    if (csv && std::getline(csv, header)) {
      return header;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  return std::nullopt;
}

bool runProcessBriefly(const std::filesystem::path& binary,
                       const std::vector<std::string>& args,
                       std::chrono::milliseconds runtime = std::chrono::milliseconds(200)) {
  if (!std::filesystem::exists(binary)) {
    INFO(std::string("Skipping missing binary: ") + binary.string());
    return false;
  }
  try {
    Poco::ProcessHandle handle(Poco::Process::launch(binary.string(), args));
    std::this_thread::sleep_for(runtime);
    try {
      Poco::Process::requestTermination(handle.id());
    } catch (...) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    try {
      Poco::Process::kill(handle);
    } catch (...) {
    }
    handle.wait();
    return true;
  } catch (const Poco::Exception& ex) {
    INFO(std::string("Failed to launch ") + binary.string() + ": " + ex.displayText());
  } catch (const std::exception& ex) {
    INFO(std::string("Failed to launch ") + binary.string() + ": " + ex.what());
  }
  return false;
}

struct EnvGuard {
  std::string key;
  std::string previous;
  bool had_previous{false};
  EnvGuard(std::string k, std::string value) : key(std::move(k)) {
    const char* current = std::getenv(key.c_str());
    if (current) {
      had_previous = true;
      previous = current;
    }
    setenv(key.c_str(), value.c_str(), 1);
  }
  ~EnvGuard() {
    if (had_previous) {
      setenv(key.c_str(), previous.c_str(), 1);
    } else {
      unsetenv(key.c_str());
    }
  }
};

}  // namespace

TEST_CASE("aggregator_service main handles graceful shutdown with empty config") {
  auto binary = binaryPath("services/aggregator_service/aggregator_service");
  if (!std::filesystem::exists(binary)) {
    INFO("aggregator_service binary missing, skipping");
    return;
  }
  auto temp_dir = std::filesystem::temp_directory_path() / uniqueSuffix("agg_service_smoke_");
  std::filesystem::create_directories(temp_dir);
  auto config_path = temp_dir / "config.json";
  {
    std::ofstream cfg(config_path);
    cfg << "{\n";
    cfg << "  \"symbol\": \"BTCUSDT\",\n";
    cfg << "  \"grpc\": {\n";
    cfg << "    \"listen_address\": \"127.0.0.1\",\n";
    cfg << "    \"port\": 0,\n";
    cfg << "    \"auth_token\": \"\"\n";
    cfg << "  },\n";
    cfg << "  \"feeds\": []\n";
    cfg << "}\n";
  }
  EnvGuard wait_guard("HERMENEUTIC_WAIT_FOR_FEEDS", "0");
  CHECK(runProcessBriefly(binary, {config_path.string()}, std::chrono::milliseconds(300)));
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("bbo_service main writes CSV header on startup") {
  auto binary = binaryPath("services/bbo_service/bbo_service");
  if (!std::filesystem::exists(binary)) {
    INFO("bbo_service binary missing, skipping");
    return;
  }
  auto temp_dir = std::filesystem::temp_directory_path() / uniqueSuffix("bbo_service_smoke_");
  std::filesystem::create_directories(temp_dir);
  auto csv_path = temp_dir / "bbo.csv";
  CHECK(runProcessBriefly(binary,
                          {"127.0.0.1:65535", "", "BTCUSDT", csv_path.string()},
                          std::chrono::milliseconds(200)));
  auto header = waitForCsvHeader(csv_path);
  CHECK(header.has_value());
  CHECK(*header ==
        "timestamp_ns,symbol,best_bid_price,best_bid_quantity,best_ask_price,best_ask_quantity,exchange_count,"
        "last_feed_timestamp_ns,last_local_timestamp_ns,min_feed_timestamp_ns,max_feed_timestamp_ns,"
        "min_local_timestamp_ns,max_local_timestamp_ns,publish_timestamp_ns");
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("price_bands_service main writes CSV header on startup") {
  auto binary = binaryPath("services/price_bands_service/price_bands_service");
  if (!std::filesystem::exists(binary)) {
    INFO("price_bands_service binary missing, skipping");
    return;
  }
  auto temp_dir = std::filesystem::temp_directory_path() / uniqueSuffix("price_bands_smoke_");
  std::filesystem::create_directories(temp_dir);
  auto csv_path = temp_dir / "price_bands.csv";
  CHECK(runProcessBriefly(binary,
                          {"127.0.0.1:65535", "", "BTCUSDT", csv_path.string()},
                          std::chrono::milliseconds(200)));
  auto header = waitForCsvHeader(csv_path);
  CHECK(header.has_value());
  CHECK(*header ==
        "timestamp_ns,symbol,offset_bps,bid_price,ask_price,last_feed_timestamp_ns,last_local_timestamp_ns,"
        "min_feed_timestamp_ns,max_feed_timestamp_ns,min_local_timestamp_ns,max_local_timestamp_ns,publish_timestamp_ns");
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);
}

TEST_CASE("volume_bands_service main writes CSV header on startup") {
  auto binary = binaryPath("services/volume_bands_service/volume_bands_service");
  if (!std::filesystem::exists(binary)) {
    INFO("volume_bands_service binary missing, skipping");
    return;
  }
  auto temp_dir = std::filesystem::temp_directory_path() / uniqueSuffix("volume_bands_smoke_");
  std::filesystem::create_directories(temp_dir);
  auto csv_path = temp_dir / "volume_bands.csv";
  CHECK(runProcessBriefly(binary,
                          {"127.0.0.1:65535", "", "BTCUSDT", csv_path.string()},
                          std::chrono::milliseconds(200)));
  auto header = waitForCsvHeader(csv_path);
  CHECK(header.has_value());
  CHECK(*header ==
        "timestamp_ns,symbol,notional,bid_price,ask_price,last_feed_timestamp_ns,last_local_timestamp_ns,"
        "min_feed_timestamp_ns,max_feed_timestamp_ns,min_local_timestamp_ns,max_local_timestamp_ns,publish_timestamp_ns");
  std::error_code ec;
  std::filesystem::remove_all(temp_dir, ec);
}
