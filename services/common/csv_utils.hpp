#pragma once

#include <filesystem>
#include <string_view>

namespace hermeneutic::services {

// Ensure that the CSV file at `csv_path` begins with `header_line`. When the file
// is missing, empty, or contains stray binary data (for example leading NUL
// bytes), the function rewrites the file so the header becomes the first line
// and attempts to preserve the existing rows. Returns true on success.
bool ensureCsvHasHeader(const std::filesystem::path& csv_path, std::string_view header_line);

}  // namespace hermeneutic::services

