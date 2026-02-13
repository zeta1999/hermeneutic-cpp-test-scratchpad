#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "hermeneutic/aggregator/config.hpp"

namespace hermeneutic::services::aggregator_service {

using HostResolver = std::function<std::optional<std::string>(const std::string& host)>;
using Sleeper = std::function<void(std::chrono::milliseconds)>;

bool shouldWaitForFeeds(const char* env_value);
bool shouldWaitForFeeds();

std::vector<std::string> collectFeedHosts(const hermeneutic::aggregator::AggregatorConfig& config);

void waitForFeedHosts(const hermeneutic::aggregator::AggregatorConfig& config,
                      std::atomic<bool>& running,
                      const HostResolver& resolver,
                      std::chrono::milliseconds retry_delay = std::chrono::seconds(1),
                      Sleeper sleeper = {});

}  // namespace hermeneutic::services::aggregator_service
