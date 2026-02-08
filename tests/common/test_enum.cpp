#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>

#include "hermeneutic/common/enum.hpp"

namespace {
HERMENEUTIC_ENUM(Toto, ta, tb, tc);
HERMENEUTIC_ENUM_INSTANTIATE(Toto);
}  // namespace

TEST_CASE("enum string round-trips") {
  CHECK(std::string(TotoToString(Toto::ta)) == "ta");
  CHECK(std::string(TotoToString(Toto::tb)) == "tb");
  CHECK(StringToToto("tc") == Toto::tc);
}

TEST_CASE("enum dynamic additions") {
  Toto dummy = Toto::ta;
  auto extra = AddDynamicElement(dummy, "td");
  CHECK(std::string(TotoToString(extra)) == "td");
  CHECK(StringToToto("td") == extra);
}
