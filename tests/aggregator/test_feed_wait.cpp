#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "tests/include/doctest_config.hpp"

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "services/aggregator_service/feed_wait.hpp"

using hermeneutic::aggregator::AggregatorConfig;
using hermeneutic::aggregator::FeedConfig;
using hermeneutic::services::aggregator_service::collectFeedHosts;
using hermeneutic::services::aggregator_service::shouldWaitForFeeds;
using hermeneutic::services::aggregator_service::waitForFeedHosts;

namespace {
AggregatorConfig makeConfig(std::vector<std::string> urls) {
  AggregatorConfig config;
  for (std::size_t i = 0; i < urls.size(); ++i) {
    config.feeds.push_back(FeedConfig{.name = "feed" + std::to_string(i), .url = urls[i], .auth_token = ""});
  }
  return config;
}
}

TEST_CASE("shouldWaitForFeeds handles common flag values") {
  CHECK(shouldWaitForFeeds(nullptr));
  CHECK(shouldWaitForFeeds(""));
  CHECK(shouldWaitForFeeds("1"));
  CHECK(shouldWaitForFeeds("true"));
  CHECK(shouldWaitForFeeds("YES"));
  CHECK(!shouldWaitForFeeds("0"));
  CHECK(!shouldWaitForFeeds("false"));
  CHECK(!shouldWaitForFeeds("No"));
  CHECK(!shouldWaitForFeeds("off"));
  CHECK(!shouldWaitForFeeds(" OFF \t"));
}

TEST_CASE("collectFeedHosts deduplicates hosts in insertion order") {
  auto config = makeConfig({
      "ws://alpha.example.com:1234/feed0",
      "ws://beta.example.com/feed1",
      "wss://alpha.example.com/feed2",
      "invalid-url",
  });
  auto hosts = collectFeedHosts(config);
  REQUIRE(hosts.size() == 2);
  CHECK(hosts[0] == "alpha.example.com");
  CHECK(hosts[1] == "beta.example.com");
}

TEST_CASE("waitForFeedHosts retries until resolver succeeds") {
  using namespace std::chrono_literals;
  auto config = makeConfig({"ws://alpha.example.com:1234/feed0", "ws://beta.example.com/feed1"});
  std::atomic<bool> running{true};
  std::vector<std::string> attempts;
  int alpha_failures = 0;
  auto resolver = [&](const std::string& host) -> std::optional<std::string> {
    attempts.push_back(host);
    if (host == "alpha.example.com" && alpha_failures++ == 0) {
      return std::string("DNS not ready");
    }
    return std::nullopt;
  };

  waitForFeedHosts(config, running, resolver, 0ms, [](auto) {});

  REQUIRE(attempts.size() == 3);
  CHECK(attempts[0] == "alpha.example.com");
  CHECK(attempts[1] == "alpha.example.com");
  CHECK(attempts[2] == "beta.example.com");
}

TEST_CASE("waitForFeedHosts stops when running flag cleared") {
  using namespace std::chrono_literals;
  auto config = makeConfig({"ws://alpha.example.com:1234/feed0", "ws://beta.example.com/feed1"});
  std::atomic<bool> running{true};
  std::vector<std::string> attempts;
  auto resolver = [&](const std::string& host) -> std::optional<std::string> {
    attempts.push_back(host);
    running.store(false);
    return std::string("unreachable");
  };

  waitForFeedHosts(config, running, resolver, 0ms, [](auto) {});

  REQUIRE(attempts.size() == 1);
  CHECK(attempts[0] == "alpha.example.com");
}
