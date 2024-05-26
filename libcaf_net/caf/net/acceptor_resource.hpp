// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/accept_event.hpp"

#include "caf/async/fwd.hpp"
#include "caf/fwd.hpp"

namespace caf::net {

/// A consumer resource for events from an acceptor. Each event contains a
/// producer and a consumer resource for writing to and reading from the
/// accepted connection.
template <class ItemType, class... Ts>
using acceptor_resource
  = async::consumer_resource<accept_event<ItemType, Ts...>>;

} // namespace caf::net
