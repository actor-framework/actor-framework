// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"

namespace caf {

/// @relates local_actor
/// Default handler function that leaves messages in the mailbox.
/// Can also be used inside custom message handlers to signalize
/// skipping to the runtime.
template <class T>
class typed_stream {
public:
private:
  strong_actor_ptr source_;
  uint64_t id_;
};

} // namespace caf
