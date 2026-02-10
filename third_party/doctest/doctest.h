#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace doctest {
namespace detail {
struct ContextScope;

inline std::vector<const ContextScope*>& contextStack() {
  thread_local std::vector<const ContextScope*> stack;
  return stack;
}

inline std::string toString(const std::string& value) { return value; }
inline std::string toString(const char* value) {
  return value ? std::string(value) : std::string{};
}
template <typename T>
inline std::string toString(const T& value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

inline std::string formatCapture(const char* expression, const std::string& value) {
  return std::string(expression) + " = " + value;
}
template <typename T>
inline std::string formatCapture(const char* expression, const T& value) {
  return formatCapture(expression, toString(value));
}

struct ContextScope {
  explicit ContextScope(std::string message) : message_(std::move(message)) {
    contextStack().push_back(this);
  }
  ContextScope(const ContextScope&) = delete;
  ContextScope& operator=(const ContextScope&) = delete;
  ~ContextScope() {
    auto& stack = contextStack();
    if (!stack.empty() && stack.back() == this) {
      stack.pop_back();
    }
  }

  const std::string& message() const { return message_; }

 private:
  std::string message_;
};

inline void reportContextMessages() {
  for (const auto* scope : contextStack()) {
    if (scope != nullptr) {
      std::cerr << "  INFO: " << scope->message() << std::endl;
    }
  }
}

struct TestCase {
  std::string name;
  void (*func)();
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> instance;
  return instance;
}

inline int& failureCount() {
  static int count = 0;
  return count;
}

inline void registerTest(std::string name, void (*func)()) {
  registry().push_back(TestCase{std::move(name), func});
}

inline void reportFailure(const char* expr, const char* file, int line) {
  std::cerr << file << ':' << line << ": CHECK(" << expr << ") failed" << std::endl;
  reportContextMessages();
  ++failureCount();
}

struct TestSuiteRegistrar {
  TestSuiteRegistrar(std::string name, void (*func)()) { registerTest(std::move(name), func); }
};

}  // namespace detail
}  // namespace doctest

#define DOCTEST_CONCAT_IMPL(x, y) x##y
#define DOCTEST_CONCAT(x, y) DOCTEST_CONCAT_IMPL(x, y)

#define TEST_CASE(name)                                                                                 \
  static void DOCTEST_CONCAT(doctest_test_func_, __LINE__)();                                           \
  static ::doctest::detail::TestSuiteRegistrar DOCTEST_CONCAT(doctest_registrar_, __LINE__)(name,       \
                                                                 DOCTEST_CONCAT(doctest_test_func_, __LINE__)); \
  static void DOCTEST_CONCAT(doctest_test_func_, __LINE__)()

#define CHECK(expr)                                                                                     \
  do {                                                                                                  \
    if (!(expr)) {                                                                                      \
      ::doctest::detail::reportFailure(#expr, __FILE__, __LINE__);                                      \
    }                                                                                                   \
  } while (false)

#define DOCTEST_INFO(msg)                                                                               \
  [[maybe_unused]] ::doctest::detail::ContextScope DOCTEST_CONCAT(doctest_info_scope_, __LINE__)(       \
      ::doctest::detail::toString(msg))

#define INFO(msg) DOCTEST_INFO(msg)

#define CAPTURE(expr)                                                                                   \
  [[maybe_unused]] ::doctest::detail::ContextScope DOCTEST_CONCAT(doctest_capture_scope_, __LINE__)(    \
      ::doctest::detail::formatCapture(#expr, expr))

#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
int main() {
  for (const auto& test : ::doctest::detail::registry()) {
    if (test.func) {
      test.func();
    }
  }
  int failures = ::doctest::detail::failureCount();
  if (failures != 0) {
    std::cerr << failures << " test(s) failed" << std::endl;
  }
  return failures == 0 ? 0 : 1;
}
#endif
