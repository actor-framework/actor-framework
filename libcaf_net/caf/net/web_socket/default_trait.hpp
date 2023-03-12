// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/byte_span.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/web_socket/frame.hpp"

#include <string_view>
#include <vector>

namespace caf::net::web_socket {

class CAF_NET_EXPORT default_trait {
public:
  /// The input type of the application, i.e., what that flows from the
  /// WebSocket to the application layer.
  using input_type = frame;

  /// The output type of the application, i.e., what flows from the application
  /// layer to the WebSocket.
  using output_type = frame;

  /// A resource for consuming input_type elements.
  using input_resource = async::consumer_resource<input_type>;

  /// A resource for producing output_type elements.
  using output_resource = async::producer_resource<output_type>;

  /// An accept event from the server to transmit read and write handles.
  template <class... Ts>
  using accept_event = cow_tuple<input_resource, output_resource, Ts...>;

  /// A resource for consuming accept events.
  template <class... Ts>
  using acceptor_resource = async::consumer_resource<accept_event<Ts...>>;

  bool converts_to_binary(const output_type& x);

  bool convert(const output_type& x, byte_buffer& bytes);

  bool convert(const output_type& x, std::vector<char>& text);

  bool convert(const_byte_span bytes, input_type& x);

  bool convert(std::string_view text, input_type& x);

  error last_error();
};

} // namespace caf::net::web_socket
