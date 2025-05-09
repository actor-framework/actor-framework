// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <string>

namespace caf::net::ssl {

/// Bitmask type for the SSL verification mode.
enum class verify_t {};

namespace verify {

CAF_NET_EXPORT extern const verify_t none;

CAF_NET_EXPORT extern const verify_t peer;

CAF_NET_EXPORT extern const verify_t fail_if_no_peer_cert;

CAF_NET_EXPORT extern const verify_t client_once;

} // namespace verify

constexpr int to_integer(verify_t x) {
  return static_cast<int>(x);
}

inline verify_t& operator|=(verify_t& x, verify_t y) noexcept {
  return x = static_cast<verify_t>(to_integer(x) | to_integer(y));
}

constexpr verify_t operator|(verify_t x, verify_t y) noexcept {
  return static_cast<verify_t>(to_integer(x) | to_integer(y));
}

} // namespace caf::net::ssl
