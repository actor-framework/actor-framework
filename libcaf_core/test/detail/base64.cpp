// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.base64

#include "caf/detail/base64.hpp"

#include "core-test.hpp"

#include <string_view>

using namespace caf;
using namespace std::literals;
using namespace std::literals::string_literals;

using caf::detail::base64;

CAF_TEST(encoding) {
  CHECK_EQ(base64::encode("A"sv), "QQ=="sv);
  CHECK_EQ(base64::encode("AB"sv), "QUI="sv);
  CHECK_EQ(base64::encode("ABC"sv), "QUJD"sv);
  CHECK_EQ(base64::encode("https://actor-framework.org"sv),
           "aHR0cHM6Ly9hY3Rvci1mcmFtZXdvcmsub3Jn"sv);
}

CAF_TEST(decoding) {
  CHECK_EQ(base64::decode("QQ=="sv), "A"s);
  CHECK_EQ(base64::decode("QUI="sv), "AB"s);
  CHECK_EQ(base64::decode("QUJD"sv), "ABC"s);
  CHECK_EQ(base64::decode("aHR0cHM6Ly9hY3Rvci1mcmFtZXdvcmsub3Jn"sv),
           "https://actor-framework.org"s);
}
