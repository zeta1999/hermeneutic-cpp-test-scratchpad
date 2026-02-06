#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace doctest {
namespace detail {
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
