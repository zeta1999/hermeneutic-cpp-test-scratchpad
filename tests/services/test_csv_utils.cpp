#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

#include "services/common/csv_utils.hpp"

namespace fs = std::filesystem;

namespace {
fs::path createTempDirectory() {
  const auto base = fs::temp_directory_path();
  std::random_device rd;
  std::uniform_int_distribution<int> dist(0, 15);
  constexpr std::string_view kHex = "0123456789abcdef";

  for (int attempt = 0; attempt < 32; ++attempt) {
    std::string suffix(6, '0');
    for (auto &ch : suffix) {
      ch = kHex[static_cast<size_t>(dist(rd))];
    }
    auto candidate = base / ("hermeneutic-csv-" + suffix);
    std::error_code ec;
    if (fs::create_directories(candidate, ec) && !ec) {
      return candidate;
    }
  }

  throw std::runtime_error("failed to create temporary directory");
}

struct TempDir {
  fs::path path;
  TempDir() : path(createTempDirectory()) {}
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

constexpr std::string_view kBboHeader =
    "timestamp_ns,symbol,best_bid_price,best_bid_quantity,best_ask_price,best_ask_quantity,exchange_count,"
    "last_feed_timestamp_ns,last_local_timestamp_ns,min_feed_timestamp_ns,max_feed_timestamp_ns,"
    "min_local_timestamp_ns,max_local_timestamp_ns,publish_timestamp_ns";

std::string readFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
}  // namespace

TEST_CASE("services/csv_utils writes header to new files") {
  TempDir dir;
  auto csv_path = dir.path / "bbo.csv";
  CHECK(hermeneutic::services::ensureCsvHasHeader(csv_path, kBboHeader));

  std::ifstream csv(csv_path);
  std::string header_line;
  CHECK(std::getline(csv, header_line));
  CHECK(header_line == kBboHeader);
}

TEST_CASE("services/csv_utils repairs files missing headers and strips NULs") {
  TempDir dir;
  auto csv_path = dir.path / "bbo.csv";
  {
    std::ofstream csv(csv_path, std::ios::binary);
    csv << std::string(16, '\0');
    csv << "1770939201796332012,BTCUSDT,30002.33,1\n";
  }

  CHECK(hermeneutic::services::ensureCsvHasHeader(csv_path, kBboHeader));

  std::ifstream csv(csv_path);
  std::string header_line;
  std::string second_line;
  CHECK(std::getline(csv, header_line));
  CHECK(std::getline(csv, second_line));
  CHECK(header_line == kBboHeader);
  CHECK(second_line == "1770939201796332012,BTCUSDT,30002.33,1");

  const auto contents = readFile(csv_path);
  CHECK(contents.find('\0') == std::string::npos);
}
