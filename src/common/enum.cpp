#include "hermeneutic/common/enum.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

extern "C" void hermeneutic_nifty_it() {}

namespace {
bool isDelimiter(char ch) {
  return ch == ',';
}

bool isWhitespace(char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}
}  // namespace

namespace hermeneutic::enum_support {

void Log(const std::string& message) {
  spdlog::info("[hermeneutic::enum] {}", message);
}

[[noreturn]] void Error(const std::string& message) {
  throw std::runtime_error(message);
}

void nifty_init() {}

void ParseEnumSpec(const char* spec, std::function<void(const char*)> callback) {
  if (spec == nullptr || !callback) {
    return;
  }

  std::string_view view(spec);
  std::string token;
  std::size_t position = 0;
  while (position < view.size()) {
    while (position < view.size() && (isWhitespace(view[position]) || isDelimiter(view[position]))) {
      ++position;
    }
    std::size_t start = position;
    while (position < view.size() && !isDelimiter(view[position])) {
      ++position;
    }
    std::size_t end = position;
    while (end > start && isWhitespace(view[end - 1])) {
      --end;
    }
    if (end > start) {
      token.assign(view.substr(start, end - start));
      callback(token.c_str());
    }
  }
}

}  // namespace hermeneutic::enum_support
