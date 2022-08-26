// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/type_id.hpp"

namespace caf {

/// @relates local_actor
/// Default handler function that leaves messages in the mailbox.
/// Can also be used inside custom message handlers to signalize
/// skipping to the runtime.
class CAF_CORE_EXPORT stream {
public:
  type_id_t type() const noexcept {
    return type_;
  }

private:
  type_id_t type_;
  strong_actor_ptr source_;
  uint64_t id_;
};

} // namespace caf
