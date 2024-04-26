// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/fwd.hpp"

namespace caf::net {

/// A single event from an acceptor. Contains a producer and a consumer resource
/// for writing to and reading from the accepted connection.
template <class Input, class Output = Input>
using accept_event = caf::cow_tuple<caf::async::consumer_resource<Input>,
                                    caf::async::producer_resource<Output>>;

} // namespace caf::net
