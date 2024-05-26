// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/base64.hpp"

#include "caf/test/test.hpp"

#include <string_view>

using namespace caf;
using namespace std::literals;
using namespace std::literals::string_literals;

using caf::detail::base64;

namespace {

TEST("encoding") {
  check_eq(base64::encode("A"sv), "QQ=="sv);
  check_eq(base64::encode("AB"sv), "QUI="sv);
  check_eq(base64::encode("ABC"sv), "QUJD"sv);
  check_eq(base64::encode("https://actor-framework.org"sv),
           "aHR0cHM6Ly9hY3Rvci1mcmFtZXdvcmsub3Jn"sv);
}

TEST("decoding") {
  check_eq(base64::decode("QQ=="sv), "A"s);
  check_eq(base64::decode("QUI="sv), "AB"s);
  check_eq(base64::decode("QUJD"sv), "ABC"s);
  check_eq(base64::decode("aHR0cHM6Ly9hY3Rvci1mcmFtZXdvcmsub3Jn"sv),
           "https://actor-framework.org"s);
}

} // namespace
