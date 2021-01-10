// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <cstdint>

#include "caf/detail/comparable.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/hex_formatted.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/message_type.hpp"
#include "caf/type_id.hpp"

namespace caf::net::basp {

/// @addtogroup BASP

/// The header of a Binary Actor System Protocol (BASP) message.
struct CAF_NET_EXPORT header : detail::comparable<header> {
  // -- constructors, destructors, and assignment operators --------------------

  constexpr header() noexcept
    : type(message_type::handshake), payload_len(0), operation_data(0) {
    // nop
  }

  constexpr header(message_type type, uint32_t payload_len,
                   uint64_t operation_data) noexcept
    : type(type), payload_len(payload_len), operation_data(operation_data) {
    // nop
  }

  header(const header&) noexcept = default;

  header& operator=(const header&) noexcept = default;

  // -- factory functions ------------------------------------------------------

  /// @pre `bytes.size() == header_size`
  static header from_bytes(span<const byte> bytes);

  // -- comparison -------------------------------------------------------------

  int compare(header other) const noexcept;

  // -- member variables -------------------------------------------------------

  /// Denotes the BASP operation and how `operation_data` gets interpreted.
  message_type type;

  /// Stores the size in bytes for the payload that follows this header.
  uint32_t payload_len;

  /// Stores type-specific information such as the BASP version in handshakes.
  uint64_t operation_data;
};

/// Serializes a header to a byte representation.
/// @relates header
CAF_NET_EXPORT std::array<byte, header_size> to_bytes(header x);

/// Serializes a header to a byte representation.
/// @relates header
CAF_NET_EXPORT void to_bytes(header x, byte_buffer& buf);

/// @relates header
template <class Inspector>
bool inspect(Inspector& f, header& x) {
  return f.object(x).fields(f.field("type", x.type),
                            f.field("payload_len", x.payload_len),
                            f.field("operation_data", x.operation_data));
}

/// @}

} // namespace caf::net::basp

namespace caf {

template <>
struct type_name<net::basp::header> {
  static constexpr string_view value = "caf::net::basp::header";
};

} // namespace caf
