/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/local_group_module.hpp"

#include "caf/actor_system.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/make_counted.hpp"
#include "caf/send.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/string_algorithms.hpp"

namespace caf::detail {

// -- local group intermediary -------------------------------------------------

local_group_module::intermediary_actor_state::intermediary_actor_state(
  event_based_actor* self, abstract_group_ptr gptr)
  : self(self), gptr(std::move(gptr)) {
  // nop
}

behavior local_group_module::intermediary_actor_state::make_behavior() {
  self->set_down_handler([this](const down_msg& dm) {
    if (auto ptr = dm.source.get())
      gptr->unsubscribe(ptr);
  });
  return {
    [this](join_atom, const strong_actor_ptr& other) {
      CAF_LOG_TRACE(CAF_ARG(other));
      if (other) {
        gptr->subscribe(other);
        self->monitor(other);
      }
    },
    [this](leave_atom, const strong_actor_ptr& other) {
      CAF_LOG_TRACE(CAF_ARG(other));
      if (other) {
        gptr->unsubscribe(other.get());
        self->demonitor(other);
      }
    },
    [this](forward_atom, const message& what) {
      CAF_LOG_TRACE(CAF_ARG(what));
      gptr->enqueue(self->current_sender(), make_message_id(), what,
                    self->context());
    },
  };
}

// -- local group impl ---------------------------------------------------------

local_group_module::impl::impl(group_module_ptr mod, std::string id)
  : super(mod, std::move(id), mod->system().node()) {
  CAF_LOG_DEBUG("created new local group:" << identifier_);
}

local_group_module::impl::~impl() {
  CAF_LOG_DEBUG("destroyed local group:" << identifier_);
}

void local_group_module::impl::enqueue(strong_actor_ptr sender, message_id mid,
                                       message content, execution_unit* host) {
  std::unique_lock<std::mutex> guard{mtx_};
  for (auto subscriber : subscribers_)
    subscriber->enqueue(sender, mid, content, host);
}

bool local_group_module::impl::subscribe(strong_actor_ptr who) {
  std::unique_lock<std::mutex> guard{mtx_};
  return !stopped_ && subscribers_.emplace(who).second;
}

void local_group_module::impl::unsubscribe(const actor_control_block* who) {
  std::unique_lock<std::mutex> guard{mtx_};
  // Note: can't call erase with `who` directly, because only set::find has an
  // overload for any K that is less-comparable to Key.
  if (auto i = subscribers_.find(who); i != subscribers_.end())
    subscribers_.erase(i);
}

actor local_group_module::impl::intermediary() const noexcept {
  std::unique_lock<std::mutex> guard{mtx_};
  return intermediary_;
}

void local_group_module::impl::stop() {
  CAF_LOG_DEBUG("stop local group:" << identifier_);
  auto hdl = actor{};
  auto subs = subscriber_set{};
  {
    using std::swap;
    std::unique_lock<std::mutex> guard{mtx_};
    if (stopped_)
      return;
    stopped_ = true;
    swap(subs, subscribers_);
    swap(hdl, intermediary_);
  }
  anon_send_exit(hdl, exit_reason::user_shutdown);
}

// -- local group module -------------------------------------------------------

local_group_module::local_group_module(actor_system& sys)
  : super(sys, "local") {
  // nop
}

local_group_module::~local_group_module() {
  stop();
}

expected<group> local_group_module::get(const std::string& group_name) {
  std::unique_lock<std::mutex> guard{mtx_};
  if (stopped_) {
    return make_error(sec::runtime_error,
                      "cannot get a group from on a stopped module");
  } else if (auto i = instances_.find(group_name); i != instances_.end()) {
    return group{i->second.get()};
  } else {
    auto ptr = make_counted<impl>(this, group_name);
    ptr->intermediary_ = system().spawn<intermediary_actor, hidden>(ptr);
    instances_.emplace(group_name, ptr);
    return group{ptr.release()};
  }
}

void local_group_module::stop() {
  instances_map tmp;
  {
    using std::swap;
    std::unique_lock<std::mutex> guard{mtx_};
    if (stopped_)
      return;
    swap(instances_, tmp);
    stopped_ = true;
  }
  for (auto& kvp : tmp)
    kvp.second->stop();
}

} // namespace caf::detail
