// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"

namespace caf::net::ssl {

/// Configures the allowed DTLS versions on a @ref caf::net::ssl::context.
enum class dtls {
  any,
  v1_0,
  v1_2,
};

/// @relates dtls
CAF_NET_EXPORT std::string to_string(dtls);

/// @relates dtls
CAF_NET_EXPORT bool from_string(std::string_view, dtls&);

/// @relates dtls
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<dtls>, dtls&);

/// @relates dtls
template <class Inspector>
bool inspect(Inspector& f, dtls& x) {
  return default_enum_inspect(f, x);
}

/// @relates dtls
CAF_NET_EXPORT int native(dtls);

} // namespace caf::net::ssl
