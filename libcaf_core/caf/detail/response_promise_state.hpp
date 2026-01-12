// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/message_id.hpp"
#include "caf/ref_counted.hpp"

namespace caf::detail {

class CAF_CORE_EXPORT response_promise_state : public ref_counted {
public:
  response_promise_state() = default;
  response_promise_state(const response_promise_state&) = delete;
  response_promise_state& operator=(const response_promise_state&) = delete;
  ~response_promise_state();

  void cancel();

  void deliver_impl(message msg);

  void delegate_impl(abstract_actor* receiver, message msg);

  strong_actor_ptr self;
  strong_actor_ptr source;
  message_id id;
};

} // namespace caf::detail
