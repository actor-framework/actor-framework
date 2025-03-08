#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace caf::net::lp {

/// Configures the integer type used as the length prefix.
enum class size_field_type : uint8_t {
  /// Configures a length prefix with a single byte.
  u1,
  /// Configures a length prefix with two bytes.
  u2,
  /// Configures a length prefix with four bytes.
  u4,
  /// Configures a length prefix with eight bytes.
  u8,
};

/// @relates size_field_type
CAF_NET_EXPORT std::string to_string(size_field_type);

/// @relates size_field_type
CAF_NET_EXPORT bool from_string(std::string_view, size_field_type&);

/// @relates size_field_type
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<size_field_type>,
                                 size_field_type&);

/// @relates size_field_type
template <class Inspector>
bool inspect(Inspector& f, size_field_type& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net::lp
