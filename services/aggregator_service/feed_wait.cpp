#include "services/aggregator_service/feed_wait.hpp"

#include <Poco/URI.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <thread>
#include <unordered_set>

#include <spdlog/spdlog.h>

namespace hermeneutic::services::aggregator_service {
namespace {

std::string normalizeEnvValue(const char* env_value) {
  if (!env_value) {
    return {};
  }
  std::string value(env_value);
  value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char ch) {
               return std::isspace(ch) != 0;
             }),
             value.end());
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

Sleeper defaultSleeper() {
  return [](std::chrono::milliseconds duration) {
    if (duration.count() > 0) {
      std::this_thread::sleep_for(duration);
    }
  };
}

}  // namespace

bool shouldWaitForFeeds(const char* env_value) {
  const auto normalized = normalizeEnvValue(env_value);
  if (normalized.empty()) {
    return true;
  }
  return !(normalized == "0" || normalized == "false" || normalized == "no" || normalized == "off");
}

bool shouldWaitForFeeds() {
  return shouldWaitForFeeds(std::getenv("HERMENEUTIC_WAIT_FOR_FEEDS"));
}

std::vector<std::string> collectFeedHosts(const hermeneutic::aggregator::AggregatorConfig& config) {
  std::vector<std::string> hosts;
  std::unordered_set<std::string> seen;
  for (const auto& feed : config.feeds) {
    try {
      Poco::URI uri(feed.url);
      auto host = uri.getHost();
      if (!host.empty() && seen.insert(host).second) {
        hosts.push_back(host);
      }
    } catch (const std::exception& ex) {
      spdlog::warn("Failed to parse feed URL {}: {}", feed.url, ex.what());
    }
  }
  return hosts;
}

void waitForFeedHosts(const hermeneutic::aggregator::AggregatorConfig& config,
                      std::atomic<bool>& running,
                      const HostResolver& resolver,
                      std::chrono::milliseconds retry_delay,
                      Sleeper sleeper) {
  if (!resolver) {
    spdlog::warn("No resolver provided for feed host wait");
    return;
  }
  auto sleep_fn = sleeper ? sleeper : defaultSleeper();
  for (const auto& host : collectFeedHosts(config)) {
    while (running.load()) {
      auto error = resolver(host);
      if (!error.has_value()) {
        spdlog::info("Feed host {} resolved", host);
        break;
      }
      spdlog::info("Waiting for feed host {}: {}", host, *error);
      if (!running.load()) {
        break;
      }
      if (retry_delay.count() > 0) {
        sleep_fn(retry_delay);
      }
    }
    if (!running.load()) {
      spdlog::info("Feed host wait interrupted");
      return;
    }
  }
}

}  // namespace hermeneutic::services::aggregator_service
