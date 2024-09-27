// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"

namespace caf::net::ssl {

/// Configures the allowed TLS versions on a @ref caf::net::ssl::context.
enum class tls {
  any,
  v1_0,
  v1_1,
  v1_2,
  v1_3,
};

/// @relates tls
CAF_NET_EXPORT std::string to_string(tls);

/// @relates tls
CAF_NET_EXPORT bool from_string(std::string_view, tls&);

/// @relates tls
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<tls>, tls&);

/// @relates tls
template <class Inspector>
bool inspect(Inspector& f, tls& x) {
  return default_enum_inspect(f, x);
}

/// @relates tls
CAF_NET_EXPORT int native(tls);

/// @relates tls
CAF_NET_EXPORT bool has(tls, tls, tls);

} // namespace caf::net::ssl
