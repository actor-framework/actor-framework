/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/decorator/splitter.hpp"

#include "caf/actor_system.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/response_promise.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/default_attachable.hpp"

#include "caf/detail/disposer.hpp"
#include "caf/detail/sync_request_bouncer.hpp"

namespace caf {
namespace decorator {

namespace {

struct splitter_state {
  response_promise rp;
  message result;
  size_t pending;
};

behavior fan_out_fan_in(stateful_actor<splitter_state>* self,
                        const std::vector<strong_actor_ptr>& workers) {
  auto f = [=](local_actor*, message_view& x) -> result<message> {
    auto msg = x.move_content_to_message();
    self->state.rp = self->make_response_promise();
    self->state.pending = workers.size();
    // request().await() has LIFO ordering
    for (auto i = workers.rbegin(); i != workers.rend(); ++i)
      // TODO: maybe infer some useful timeout or use config parameter?
      self->request(actor_cast<actor>(*i), infinite, msg).await(
        [=]() {
          // nop
        },
        [=](error& err) mutable {
          if (err == sec::unexpected_response) {
            self->state.result += std::move(err.context());
            if (--self->state.pending == 0)
              self->state.rp.deliver(std::move(self->state.result));
          } else {
            self->state.rp.deliver(err);
            self->quit();
          }
        }
      );
    return delegated<message>{};
  };
  self->set_default_handler(f);
  return [] {
    // nop
  };
}

} // namespace <anonymous>

splitter::splitter(std::vector<strong_actor_ptr> workers,
                   message_types_set msg_types)
    : monitorable_actor(actor_config{}.add_flag(is_actor_dot_decorator_flag)),
      num_workers(workers.size()),
      workers_(std::move(workers)),
      msg_types_(std::move(msg_types)) {
  // composed actor has dependency on constituent actors by default;
  // if either constituent actor is already dead upon establishing
  // the dependency, the actor is spawned dead
  auto addr = address();
  for (auto& worker : workers_)
    worker->get()->attach(
      default_attachable::make_monitor(actor_cast<actor_addr>(worker), addr));
}

void splitter::enqueue(mailbox_element_ptr what, execution_unit* context) {
  auto down_msg_handler = [&](down_msg& dm) {
    // quit if any worker fails
    cleanup(std::move(dm.reason), context);
  };
  if (handle_system_message(*what, context, false, down_msg_handler))
    return;
  std::vector<strong_actor_ptr> workers;
  workers.reserve(num_workers);
  error fail_state;
  shared_critical_section([&] {
    workers = workers_;
    fail_state = fail_state_;
  });
  if (workers.empty()) {
    bounce(what, fail_state);
    return;
  }
  auto helper = context->system().spawn(fan_out_fan_in, std::move(workers));
  helper->enqueue(std::move(what), context);
}

splitter::message_types_set splitter::message_types() const {
  return msg_types_;
}

} // namespace decorator
} // namespace caf
