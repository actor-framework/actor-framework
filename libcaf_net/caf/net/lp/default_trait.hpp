// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/fwd.hpp"

#include <string_view>
#include <vector>

namespace caf::net::lp {

/// A default trait type for binary protocols that uses @ref frame as both input
/// and output types and provides async::consumer_resource and
/// async::producer_resource as input_resource and output_resource types,
/// respectively.
class CAF_NET_EXPORT default_trait {
public:
  /// The input type of the application, i.e., what that flows from the socket
  /// to the application layer.
  using input_type = frame;

  /// The output type of the application, i.e., what flows from the application
  /// layer to the socket.
  using output_type = frame;

  /// A resource for consuming input_type elements.
  using input_resource = async::consumer_resource<input_type>;

  /// A resource for producing output_type elements.
  using output_resource = async::producer_resource<output_type>;

  /// An accept event from the server to transmit read and write handles.
  using accept_event = cow_tuple<input_resource, output_resource>;

  /// A resource for consuming accept events.
  using acceptor_resource = async::consumer_resource<accept_event>;

  /// Converts an output element to a byte buffer.
  /// @param x The output element to convert.
  /// @param bytes The output byte buffer.
  /// @returns `true` on success, `false` otherwise.
  bool convert(const output_type& x, byte_buffer& bytes);

  /// Converts a byte buffer to an input element.
  /// @param bytes The input byte buffer.
  /// @param x The output input element.
  /// @returns `true` on success, `false` otherwise.
  bool convert(const_byte_span bytes, input_type& x);

  /// Returns the last error that occurred.
  error last_error();
};

} // namespace caf::net::lp
