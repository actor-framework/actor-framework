// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <string>

namespace caf::net::ssl {

/// Format of keys and certificates.
enum class format {
  /// Privacy Enhanced Mail format.
  pem,
  /// Binary ASN1 format.
  asn1,
};

/// @relates format
CAF_NET_EXPORT std::string to_string(format);

/// @relates format
CAF_NET_EXPORT bool from_string(std::string_view, format&);

/// @relates format
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<format>, format&);

/// @relates format
template <class Inspector>
bool inspect(Inspector& f, format& x) {
  return default_enum_inspect(f, x);
}

/// @relates dtls
CAF_NET_EXPORT int native(format);

} // namespace caf::net::ssl
