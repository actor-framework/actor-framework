// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/version.hpp"

#include <cstdio>

namespace caf {

int version::get_major() noexcept {
  return CAF_VERSION_MAJOR;
}

int version::get_minor() noexcept {
  return CAF_VERSION_MINOR;
}

int version::get_patch() noexcept {
  return CAF_VERSION_PATCH;
}

std::string_view version::str() noexcept {
  return CAF_VERSION_STR;
}

const char* version::c_str() noexcept {
  return CAF_VERSION_STR;
}

void version::check_abi_compatibility(abi_token token) noexcept {
  if (static_cast<int>(token) != CAF_VERSION_MAJOR) {
    fprintf(stderr, "CAF ABI mismatch: expected version %d, got %d\n",
            CAF_VERSION_MAJOR, static_cast<int>(token));
    abort();
  }
}

inline namespace CAF_ABI_NAMESPACE {
version::abi_token make_abi_token() noexcept {
  return static_cast<version::abi_token>(CAF_VERSION_MAJOR);
}
} // namespace CAF_ABI_NAMESPACE

} // namespace caf
