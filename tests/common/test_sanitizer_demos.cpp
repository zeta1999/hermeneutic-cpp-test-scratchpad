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
#if 0
  CHECK(runChildAndExpectFailure(triggerAsanOverflow));
#endif
}

TEST_CASE("sanitizers/tsan_data_race_demo") {
#if 0
  CHECK(runChildAndExpectFailure(triggerTsanRace));
#endif
}
