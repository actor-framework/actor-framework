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

#include <string>
#include <condition_variable>

#include "caf/sec.hpp"
#include "caf/atom.hpp"
#include "caf/logger.hpp"
#include "caf/scheduler.hpp"
#include "caf/resumable.hpp"
#include "caf/actor_cast.hpp"
#include "caf/exit_reason.hpp"
#include "caf/local_actor.hpp"
#include "caf/actor_system.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/default_attachable.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/detail/private_thread.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/detail/default_invoke_result_visitor.hpp"

namespace caf {

// local actors are created with a reference count of one that is adjusted
// later on in spawn(); this prevents subtle bugs that lead to segfaults,
// e.g., when calling address() in the ctor of a derived class
local_actor::local_actor(actor_config& cfg)
    : monitorable_actor(cfg),
      context_(cfg.host),
      initial_behavior_fac_(std::move(cfg.init_fun)) {
  // nop
}

local_actor::~local_actor() {
  // nop
}

void local_actor::on_destroy() {
  // disable logging from this point on, because on_destroy can
  // be called after the logger is already destroyed;
  // alternatively, we would have to use a reference-counted,
  // heap-allocated logger
  CAF_SET_LOGGER_SYS(nullptr);
  if (! is_cleaned_up()) {
    on_exit();
    cleanup(exit_reason::unreachable, nullptr);
    monitorable_actor::on_destroy();
  }
}

void local_actor::request_response_timeout(const duration& d, message_id mid) {
  CAF_LOG_TRACE(CAF_ARG(d) << CAF_ARG(mid));
  if (! d.valid())
    return;
  system().scheduler().delayed_send(d, ctrl(), ctrl(), mid.response_id(),
                                    make_message(sec::request_timeout));
}

void local_actor::monitor(abstract_actor* ptr) {
  if (! ptr)
    return;
  ptr->attach(default_attachable::make_monitor(ptr->address(), address()));
}

void local_actor::demonitor(const actor_addr& whom) {
  CAF_LOG_TRACE(CAF_ARG(whom));
  auto ptr = actor_cast<strong_actor_ptr>(whom);
  if (! ptr)
    return;
  default_attachable::observe_token tk{address(), default_attachable::monitor};
  ptr->get()->detach(tk);
}

void local_actor::on_exit() {
  // nop
}


message_id local_actor::new_request_id(message_priority mp) {
  auto result = ++last_request_id_;
  return mp == message_priority::normal ? result : result.with_high_priority();
}

mailbox_element_ptr local_actor::next_message() {
  if (! is_priority_aware())
    return mailbox_element_ptr{mailbox().try_pop()};
  // we partition the mailbox into four segments in this case:
  // <-------- ! was_skipped --------> | <--------  was_skipped  -------->
  // <-- high prio --><-- low prio --> | <-- high prio --><-- low prio -->
  auto& cache = mailbox().cache();
  auto i = cache.begin();
  auto e = cache.separator();
  // read elements from mailbox if we don't have a high
  // priority message or if cache is empty
  if (i == e || ! i->is_high_priority()) {
    // insert points for high priority
    auto hp_pos = i;
    // read whole mailbox at once
    auto tmp = mailbox().try_pop();
    while (tmp) {
      cache.insert(tmp->is_high_priority() ? hp_pos : e, tmp);
      // adjust high priority insert point on first low prio element insert
      if (hp_pos == e && ! tmp->is_high_priority())
        --hp_pos;
      tmp = mailbox().try_pop();
    }
  }
  mailbox_element_ptr result;
  i = cache.begin();
  if (i != e)
    result.reset(cache.take(i));
  return result;
}

bool local_actor::has_next_message() {
  if (! is_priority_aware())
    return mailbox_.can_fetch_more();
  auto& mbox = mailbox();
  auto& cache = mbox.cache();
  return cache.begin() != cache.separator() || mbox.can_fetch_more();
}

void local_actor::push_to_cache(mailbox_element_ptr ptr) {
  CAF_ASSERT(ptr != nullptr);
  CAF_LOG_TRACE(CAF_ARG(*ptr));
  if (! is_priority_aware() || ! ptr->is_high_priority()) {
    mailbox().cache().insert(mailbox().cache().end(), ptr.release());
    return;
  }
  auto high_prio = [](const mailbox_element& val) {
    return val.is_high_priority();
  };
  auto& cache = mailbox().cache();
  auto e = cache.end();
  cache.insert(std::partition_point(cache.continuation(),
                                    e, high_prio),
               ptr.release());
}

void local_actor::send_exit(const actor_addr& whom, error reason) {
  send_exit(actor_cast<strong_actor_ptr>(whom), std::move(reason));
}

void local_actor::send_exit(const strong_actor_ptr& dest, error reason) {
  if (! dest)
    return;
  dest->get()->eq_impl(message_id::make(), nullptr, context(),
                       exit_msg{address(), std::move(reason)});
}

const char* local_actor::name() const {
  return "actor";
}

error local_actor::save_state(serializer&, const unsigned int) {
  CAF_RAISE_ERROR("local_actor::serialize called");
}

error local_actor::load_state(deserializer&, const unsigned int) {
  CAF_RAISE_ERROR("local_actor::deserialize called");
}

void local_actor::initialize() {
  // nop
}

bool local_actor::cleanup(error&& fail_state, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(fail_state));
  if (! mailbox_.closed()) {
    detail::sync_request_bouncer f{fail_state};
    mailbox_.close(f);
  }
  // tell registry we're done
  is_registered(false);
  monitorable_actor::cleanup(std::move(fail_state), host);
  return true;
}

} // namespace caf
