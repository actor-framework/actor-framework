// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"

#include <string>
#include <string_view>
#include <type_traits>

namespace caf::net::ssl {

/// Identifies the SSL/TLS implementation in use.
enum class backend {
  /// Denotes the OpenSSL library.
  openssl,
};

/// @relates backend
CAF_NET_EXPORT std::string to_string(backend);

/// @relates backend
CAF_NET_EXPORT bool from_string(std::string_view, backend&);

/// @relates backend
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<backend>, backend&);

/// @relates backend
template <class Inspector>
bool inspect(Inspector& f, backend& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net::ssl
