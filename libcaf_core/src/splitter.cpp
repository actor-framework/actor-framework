/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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
  auto f = [=](local_actor*, const type_erased_tuple* x) -> result<message> {
    auto msg = message::from(x);
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
  self->set_unexpected_handler(f);
  return [] {
    // nop
  };
  /*

      // TODO: maybe infer some useful timeout or use config parameter?
      self->request(actor_cast<actor>(*i), infinite, msg)
      .generic_await(
        [=](const message& tmp) {
          self->state.result += tmp;
          if (--self->state.pending == 0)
            self->state.rp.deliver(std::move(self->state.result));
        },
        [=](const error& err) {
          self->state.rp.deliver(err);
          self->quit();
        }
      );
  };
  set_unexpected_handler(f);
  return {
    others >> [=](const message& msg) {
      self->state.rp = self->make_response_promise();
      self->state.pending = workers.size();
      // request().await() has LIFO ordering
      for (auto i = workers.rbegin(); i != workers.rend(); ++i)
        // TODO: maybe infer some useful timeout or use config parameter?
        self->request(actor_cast<actor>(*i), infinite, msg)
        .generic_await(
          [=](const message& tmp) {
            self->state.result += tmp;
            if (--self->state.pending == 0)
              self->state.rp.deliver(std::move(self->state.result));
          },
          [=](const error& err) {
            self->state.rp.deliver(err);
            self->quit();
          }
        );
      self->unbecome();
    }
  };
*/
}

} // namespace <anonymous>

splitter::splitter(std::vector<strong_actor_ptr> workers,
                   message_types_set msg_types)
    : monitorable_actor(is_abstract_actor_flag | is_actor_dot_decorator_flag),
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
  if (! what)
    return; // not even an empty message
  auto reason = get_exit_reason();
  if (reason != exit_reason::not_exited) {
    // actor has exited
    auto& mid = what->mid;
    if (mid.is_request()) {
      // make sure that a request always gets a response;
      // the exit reason reflects the first actor on the
      // forwarding chain that is out of service
      detail::sync_request_bouncer rb{reason};
      rb(what->sender, mid);
    }
    return;
  }
  auto down_msg_handler = [&](const down_msg& dm) {
    // quit if either `f` or `g` are no longer available
    auto pred = [&](const strong_actor_ptr& x) {
      return actor_addr::compare(
               x.get(), actor_cast<actor_control_block*>(dm.source)) == 0;
    };
    if (std::any_of(workers_.begin(), workers_.end(), pred))
      monitorable_actor::cleanup(dm.reason, context);
  };
  // handle and consume the system message;
  // the only effect that MAY result from handling a system message
  // is to exit the actor if it hasn't exited already;
  // `handle_system_message()` is thread-safe, and if the actor
  // has already exited upon the invocation, nothing is done
  if (handle_system_message(*what, context, false, down_msg_handler))
    return;
  auto helper = context->system().spawn(fan_out_fan_in, workers_);
  helper->enqueue(std::move(what), context);
}

splitter::message_types_set splitter::message_types() const {
  return msg_types_;
}

} // namespace decorator
} // namespace caf
