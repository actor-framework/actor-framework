// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.base64

#include "caf/detail/base64.hpp"

#include "core-test.hpp"

using namespace caf;
using namespace caf::literals;
using namespace std::literals::string_literals;

using caf::detail::base64;

CAF_TEST(encoding) {
  CHECK_EQ(base64::encode("A"_sv), "QQ=="_sv);
  CHECK_EQ(base64::encode("AB"_sv), "QUI="_sv);
  CHECK_EQ(base64::encode("ABC"_sv), "QUJD"_sv);
  CHECK_EQ(base64::encode("https://actor-framework.org"_sv),
           "aHR0cHM6Ly9hY3Rvci1mcmFtZXdvcmsub3Jn"_sv);
}

CAF_TEST(decoding) {
  CHECK_EQ(base64::decode("QQ=="_sv), "A"s);
  CHECK_EQ(base64::decode("QUI="_sv), "AB"s);
  CHECK_EQ(base64::decode("QUJD"_sv), "ABC"s);
  CHECK_EQ(base64::decode("aHR0cHM6Ly9hY3Rvci1mcmFtZXdvcmsub3Jn"_sv),
           "https://actor-framework.org"s);
}
