// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/net/web_socket/fwd.hpp"

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

  bool converts_to_binary(const output_type& x);

  bool convert(const output_type& x, byte_buffer& bytes);

  bool convert(const output_type& x, std::vector<char>& text);

  bool convert(const_byte_span bytes, input_type& x);

  bool convert(std::string_view text, input_type& x);

  error last_error();
};

} // namespace caf::net::web_socket
