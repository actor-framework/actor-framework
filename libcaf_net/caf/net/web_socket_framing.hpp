// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/web_socket/framing.hpp"

namespace caf::net {

template <class UpperLayer>
using web_socket_framing
  [[deprecated("use caf::net::web_socket::framing instead")]]
  = web_socket::framing<UpperLayer>;

} // namespace caf::net
