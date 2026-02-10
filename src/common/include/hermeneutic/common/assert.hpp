#pragma once

#include <string_view>

namespace hermeneutic::common::detail {

[[noreturn]] void debugAssertFailure(const char* expression,
                                     const char* file,
                                     int line,
                                     std::string_view message = {});

}  // namespace hermeneutic::common::detail

#if 1
//defined(HERMENEUTIC_ENABLE_DEBUG_ASSERTS) && HERMENEUTIC_ENABLE_DEBUG_ASSERTS // TODO restore
#define HERMENEUTIC_ASSERT_DEBUG(expr, ...)                                                      \
  do {                                                                                           \
    if (!(expr)) {                                                                               \
      ::hermeneutic::common::detail::debugAssertFailure(#expr, __FILE__, __LINE__ __VA_OPT__(, __VA_ARGS__)); \
    }                                                                                            \
  } while (false)
#else
#define HERMENEUTIC_ASSERT_DEBUG(expr, ...) (static_cast<void>(sizeof(expr)))
#endif
