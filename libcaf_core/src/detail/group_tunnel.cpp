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

#include "caf/detail/group_tunnel.hpp"

namespace caf::detail {

namespace {

struct group_worker_actor_state {
  static inline const char* name = "caf.detail.group-tunnel";

  group_worker_actor_state(event_based_actor* self, group_tunnel_ptr gptr,
                           actor intermediary)
    : self(self), gptr(std::move(gptr)), intermediary(std::move(intermediary)) {
    // nop
  }

  behavior make_behavior() {
    self->set_down_handler([this](const down_msg& dm) {
      if (dm.source == intermediary) {
        gptr->stop();
      }
    });
    self->set_default_handler([this](message& msg) -> skippable_result {
      gptr->upstream_enqueue(std::move(self->current_sender()),
                             self->take_current_message_id(), std::move(msg),
                             self->context());
      return message{};
    });
    self->monitor(intermediary);
    return {
      [this](sys_atom, join_atom) {
        self->send(intermediary, join_atom_v, self->ctrl());
      },
      [this](sys_atom, leave_atom) {
        self->send(intermediary, leave_atom_v, self->ctrl());
      },
      [this](sys_atom, forward_atom, message& msg) {
        self->delegate(intermediary, forward_atom_v, std::move(msg));
      },
    };
  }

  event_based_actor* self;

  group_tunnel_ptr gptr;

  actor intermediary;
};

// A group tunnel enables remote actors to join and leave groups on this
// endpoint as well as sending message to it.
using group_worker_actor = stateful_actor<group_worker_actor_state>;

} // namespace

group_tunnel::group_tunnel(group_module_ptr mod, std::string id,
                           actor upstream_intermediary)
  : super(std::move(mod), std::move(id), upstream_intermediary.node()) {
  intermediary_ = std::move(upstream_intermediary);
  worker_ = system().spawn<group_worker_actor, hidden>(this, intermediary_);
}

group_tunnel::group_tunnel(group_module_ptr mod, std::string id, node_id nid)
  : super(std::move(mod), std::move(id), std::move(nid)) {
  // nop
}

group_tunnel::~group_tunnel() {
  // nop
}

bool group_tunnel::subscribe(strong_actor_ptr who) {
  return critical_section([this, &who] {
    auto [added, new_size] = subscribe_impl(std::move(who));
    if (added && new_size == 1)
      anon_send(worker_, sys_atom_v, join_atom_v);
    return added;
  });
}

void group_tunnel::unsubscribe(const actor_control_block* who) {
  return critical_section([this, who] {
    auto [removed, new_size] = unsubscribe_impl(who);
    if (removed && new_size == 0)
      anon_send(worker_, sys_atom_v, leave_atom_v);
  });
}

void group_tunnel::enqueue(strong_actor_ptr sender, message_id mid,
                           message content, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(content));
  std::unique_lock<std::mutex> guard{mtx_};
  if (worker_ != nullptr) {
    auto wrapped = make_message(sys_atom_v, forward_atom_v, std::move(content));
    worker_->enqueue(std::move(sender), mid, std::move(wrapped), host);
  } else if (!stopped_) {
    auto wrapped = make_message(sys_atom_v, forward_atom_v, std::move(content));
    cached_messages_.emplace_back(std::move(sender), mid, std::move(wrapped));
  }
}

void group_tunnel::stop() {
  CAF_LOG_TRACE("");
  CAF_LOG_DEBUG("stop group tunnel:" << CAF_ARG2("module", module().name())
                                     << CAF_ARG2("identifier", identifier_));
  auto worker_hdl = actor{};
  auto intermediary_hdl = actor{};
  auto subs = subscriber_set{};
  auto cache = cached_message_list{};
  auto stopped = critical_section([&, this] {
    using std::swap;
    if (!stopped_) {
      stopped_ = true;
      swap(subs, subscribers_);
      swap(worker_hdl, worker_);
      swap(intermediary_hdl, intermediary_);
      swap(cache, cached_messages_);
      return true;
    } else {
      return false;
    }
  });
  if (stopped) {
    anon_send_exit(worker_hdl, exit_reason::user_shutdown);
    if (!subs.empty()) {
      auto bye = make_message(group_down_msg{group{this}});
      for (auto& sub : subs)
        sub->enqueue(nullptr, make_message_id(), bye, nullptr);
    }
  }
}

std::string group_tunnel::stringify() const {
  std::string result;
  result = "remote:";
  result += identifier();
  result += '@';
  result += to_string(origin());
  return result;
}

void group_tunnel::upstream_enqueue(strong_actor_ptr sender, message_id mid,
                                    message content, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(sender) << CAF_ARG(content));
  super::enqueue(std::move(sender), mid, std::move(content), host);
}

bool group_tunnel::connect(actor upstream_intermediary) {
  CAF_LOG_TRACE(CAF_ARG(upstream_intermediary));
  return critical_section([this, &upstream_intermediary] {
    if (stopped_ || worker_ != nullptr) {
      return false;
    } else {
      intermediary_ = upstream_intermediary;
      worker_ = system().spawn<group_worker_actor, hidden>(
        this, upstream_intermediary);
      if (!subscribers_.empty())
        anon_send(worker_, sys_atom_v, join_atom_v);
      for (auto& [sender, mid, content] : cached_messages_)
        worker_->enqueue(std::move(sender), mid, std::move(content), nullptr);
      cached_messages_.clear();
      return true;
    }
  });
}

bool group_tunnel::connected() const noexcept {
  return critical_section([this] { return worker_ != nullptr; });
}

actor group_tunnel::worker() const noexcept {
  return critical_section([this] { return worker_; });
}

} // namespace caf::detail
