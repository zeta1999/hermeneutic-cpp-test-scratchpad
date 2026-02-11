#pragma once

#include <string_view>
#include <spdlog/spdlog.h>

namespace hermeneutic::common::detail {

[[noreturn]] void debugAssertFailure(const char* expression,
                                     const char* file,
                                     int line,
                                     std::string_view message = {});

}  // namespace hermeneutic::common::detail

#ifdef HERMENEUTIC_ENABLE_DEBUG_ASSERTS
#undef HERMENEUTIC_ENABLE_DEBUG_ASSERTS
#endif
#define HERMENEUTIC_ENABLE_DEBUG_ASSERTS 1

#if defined(HERMENEUTIC_ENABLE_DEBUG_ASSERTS) && HERMENEUTIC_ENABLE_DEBUG_ASSERTS
#define HERMENEUTIC_ASSERT_DEBUG(expr, ...)                                                      \
  do {                                                                                           \
    if (!(expr)) {                                                                               \
      ::hermeneutic::common::detail::debugAssertFailure(#expr, __FILE__, __LINE__ __VA_OPT__(, __VA_ARGS__)); \
    }                                                                                            \
  } while (false)
#else
#define HERMENEUTIC_ASSERT_DEBUG(expr, ...) (static_cast<void>(sizeof(expr)))
#endif

#if defined(HERMENEUTIC_ENABLE_DEBUG_ASSERTS) && HERMENEUTIC_ENABLE_DEBUG_ASSERTS
#define HERMENEUTIC_LOG_DEBUG(expr, ...) spdlog::critical("{}:{}: " #expr, __FILE__, __LINE__ __VA_OPT__(, __VA_ARGS__))
#else
#define HERMENEUTIC_LOG_DEBUG(expr, ...)
#endif
