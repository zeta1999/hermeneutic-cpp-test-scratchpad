#include "common/csv_utils.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <spdlog/spdlog.h>

namespace hermeneutic::services {
namespace {

void stripHeaderIfPresent(std::string& data, const std::string& header_line) {
  if (data.compare(0, header_line.size(), header_line) != 0) {
    return;
  }
  std::size_t erase_len = header_line.size();
  if (erase_len < data.size() && data[erase_len] == '\r') {
    ++erase_len;
  }
  if (erase_len < data.size() && data[erase_len] == '\n') {
    ++erase_len;
  }
  data.erase(0, erase_len);
}

bool writeHeaderOnly(const std::filesystem::path& csv_path, const std::string& header_line) {
  std::ofstream csv(csv_path, std::ios::binary | std::ios::trunc);
  if (!csv.is_open()) {
    spdlog::error("Failed to open {} for writing", csv_path.string());
    return false;
  }
  csv << header_line << '\n';
  return true;
}

}  // namespace

bool ensureCsvHasHeader(const std::filesystem::path& csv_path, std::string_view header_view) {
  const std::string header_line(header_view);

  std::error_code ec;
  const bool exists = std::filesystem::exists(csv_path, ec);
  if (!exists) {
    return writeHeaderOnly(csv_path, header_line);
  }
  const auto size = std::filesystem::file_size(csv_path, ec);
  if (ec || size == 0) {
    return writeHeaderOnly(csv_path, header_line);
  }

  std::ifstream input(csv_path, std::ios::binary);
  if (!input.is_open()) {
    spdlog::error("Failed to open {} for reading", csv_path.string());
    return false;
  }
  std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  input.close();

  if (contents.empty()) {
    return writeHeaderOnly(csv_path, header_line);
  }

  const auto newline_pos = contents.find('\n');
  std::string first_line = newline_pos == std::string::npos ? contents : contents.substr(0, newline_pos);
  if (!first_line.empty() && first_line.back() == '\r') {
    first_line.pop_back();
  }
  const bool has_null = contents.find('\0') != std::string::npos;
  if (!has_null && first_line == header_line) {
    return true;
  }

  spdlog::warn("CSV {} missing header or contains NUL bytes; rewriting header", csv_path.string());

  std::string sanitized = contents;
  sanitized.erase(std::remove(sanitized.begin(), sanitized.end(), '\0'), sanitized.end());
  stripHeaderIfPresent(sanitized, header_line);

  std::ofstream output(csv_path, std::ios::binary | std::ios::trunc);
  if (!output.is_open()) {
    spdlog::error("Failed to rewrite {}", csv_path.string());
    return false;
  }
  output << header_line << '\n';
  output << sanitized;
  return true;
}

}  // namespace hermeneutic::services
