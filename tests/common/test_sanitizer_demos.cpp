#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <csignal>
#include <thread>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define HERMENEUTIC_ASAN_ENABLED 1
#  endif
#  if __has_feature(thread_sanitizer)
#    define HERMENEUTIC_TSAN_ENABLED 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#  define HERMENEUTIC_ASAN_ENABLED 1
#endif
#if defined(__SANITIZE_THREAD__)
#  define HERMENEUTIC_TSAN_ENABLED 1
#endif
#ifndef HERMENEUTIC_ASAN_ENABLED
#  define HERMENEUTIC_ASAN_ENABLED 0
#endif
#ifndef HERMENEUTIC_TSAN_ENABLED
#  define HERMENEUTIC_TSAN_ENABLED 0
#endif

#ifndef _WIN32
[[gnu::noinline]] void triggerAsanOverflow() {
  char* buffer = new char[8];
  buffer[8] = 'x';  // one byte past the allocation
  delete[] buffer;
}

[[gnu::noinline]] void triggerTsanRace() {
  static std::atomic<bool> keep_running{true};
  (void)keep_running.load();
  static int shared_counter = 0;
  auto bump = [] {
    for (int i = 0; i < 1000; ++i) {
      ++shared_counter;  // intentional data race
    }
  };
  std::thread t1(bump);
  std::thread t2(bump);
  t1.join();
  t2.join();
}

bool runChildAndExpectFailure(void (*func)()) {
  pid_t pid = fork();
  if (pid == 0) {
    func();
    _exit(0);
  }
  if (pid < 0) {
    return false;
  }
  int status = 0;
  waitpid(pid, &status, 0);
  if (WIFSIGNALED(status)) {
    return true;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status) != 0;
  }
  return false;
}
#endif

}  // namespace

TEST_CASE("sanitizers/asan_overflow_demo") {
//#if !HERMENEUTIC_ASAN_ENABLED || defined(_WIN32)
//  INFO("AddressSanitizer not enabled on this platform; skipping overflow demo");
//  return;
///#else
  CHECK(runChildAndExpectFailure(triggerAsanOverflow));
//#endif
}

TEST_CASE("sanitizers/tsan_data_race_demo") {
//#if !HERMENEUTIC_TSAN_ENABLED || defined(_WIN32)
//  INFO("ThreadSanitizer not enabled on this platform; skipping data-race demo");
//  return;
//#else
  CHECK(runChildAndExpectFailure(triggerTsanRace));
//#endif
}
