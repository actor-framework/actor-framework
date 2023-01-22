// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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

local_group_module::impl::impl(group_module_ptr mod, std::string id,
                               node_id origin)
  : super(mod, std::move(id), origin) {
  // nop
}

local_group_module::impl::impl(group_module_ptr mod, std::string id)
  : impl(mod, std::move(id), mod->system().node()) {
  CAF_LOG_DEBUG("created new local group:" << identifier_);
}

local_group_module::impl::~impl() {
  // nop
}

bool local_group_module::impl::enqueue(strong_actor_ptr sender, message_id mid,
                                       message content, execution_unit* host) {
  std::unique_lock<std::mutex> guard{mtx_};
  for (auto subscriber : subscribers_)
    subscriber->enqueue(sender, mid, content, host);
  return true;
}

bool local_group_module::impl::subscribe(strong_actor_ptr who) {
  std::unique_lock<std::mutex> guard{mtx_};
  return subscribe_impl(who).first;
}

void local_group_module::impl::unsubscribe(const actor_control_block* who) {
  std::unique_lock<std::mutex> guard{mtx_};
  unsubscribe_impl(who);
}

actor local_group_module::impl::intermediary() const noexcept {
  std::unique_lock<std::mutex> guard{mtx_};
  return intermediary_;
}

void local_group_module::impl::stop() {
  CAF_LOG_DEBUG("stop local group:" << identifier_);
  auto hdl = actor{};
  auto subs = subscriber_set{};
  auto stopped = critical_section([this, &hdl, &subs] {
    using std::swap;
    if (!stopped_) {
      stopped_ = true;
      swap(subs, subscribers_);
      swap(hdl, intermediary_);
      return true;
    } else {
      return false;
    }
  });
  if (stopped)
    anon_send_exit(hdl, exit_reason::user_shutdown);
}

std::pair<bool, size_t>
local_group_module::impl::subscribe_impl(strong_actor_ptr who) {
  if (!stopped_) {
    auto added = subscribers_.emplace(who).second;
    return {added, subscribers_.size()};
  } else {
    return {false, subscribers_.size()};
  }
}

std::pair<bool, size_t>
local_group_module::impl::unsubscribe_impl(const actor_control_block* who) {
  if (auto i = subscribers_.find(who); i != subscribers_.end()) {
    subscribers_.erase(i);
    return {true, subscribers_.size()};
  } else {
    return {false, subscribers_.size()};
  }
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
    return group{i->second};
  } else {
    auto ptr = make_counted<impl>(this, group_name);
    ptr->intermediary_ = system().spawn<intermediary_actor, hidden>(ptr);
    instances_.emplace(group_name, ptr);
    return group{std::move(ptr)};
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
