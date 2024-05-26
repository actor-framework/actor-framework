// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/version.hpp"

#include "caf/test/test.hpp"

#include "caf/detail/format.hpp"

using namespace caf;

namespace {

TEST("version functions must return the values from the build configuration") {
  check_eq(version::get_major(), CAF_VERSION_MAJOR);
  check_eq(version::get_minor(), CAF_VERSION_MINOR);
  check_eq(version::get_patch(), CAF_VERSION_PATCH);
  auto vstr = detail::format("{}.{}.{}", CAF_VERSION_MAJOR, CAF_VERSION_MINOR,
                             CAF_VERSION_PATCH);
  check_eq(version::str(), vstr);
  check_eq(version::c_str(), vstr);
  check_eq(static_cast<int>(make_abi_token()), CAF_VERSION_MAJOR);
}

} // namespace
