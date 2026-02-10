#include "hermeneutic/common/assert.hpp"

#include <cstdlib>

#include <spdlog/spdlog.h>

namespace hermeneutic::common::detail {

[[noreturn]] void debugAssertFailure(const char* expression,
                                     const char* file,
                                     int line,
                                     std::string_view message) {
  if (message.empty()) {
    spdlog::critical("Debug assertion failed: {} ({}:{})", expression, file, line);
  } else {
    spdlog::critical("Debug assertion failed: {} ({}:{}) - {}", expression, file, line, message);
  }
  std::abort();
}

}  // namespace hermeneutic::common::detail
