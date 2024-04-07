// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/version.hpp"

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

inline namespace CAF_ABI_NAMESPACE {
version::abi_token make_abi_token() noexcept {
  return static_cast<version::abi_token>(CAF_VERSION_MAJOR);
}
} // namespace CAF_ABI_NAMESPACE

} // namespace caf
