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

#include "caf/forwarding_actor_proxy.hpp"

#include "caf/send.hpp"
#include "caf/locks.hpp"
#include "caf/logger.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {

forwarding_actor_proxy::forwarding_actor_proxy(actor_config& cfg, actor mgr)
    : actor_proxy(cfg),
      manager_(mgr) {
  CAF_ASSERT(mgr != invalid_actor);
}

forwarding_actor_proxy::~forwarding_actor_proxy() {
  anon_send(manager_, make_message(delete_atom::value, node(), id()));
}

actor forwarding_actor_proxy::manager() const {
  actor result;
  {
    shared_lock<detail::shared_spinlock> guard_(manager_mtx_);
    result = manager_;
  }
  return result;
}

void forwarding_actor_proxy::manager(actor new_manager) {
  std::unique_lock<detail::shared_spinlock> guard_(manager_mtx_);
  manager_.swap(new_manager);
}

void forwarding_actor_proxy::forward_msg(strong_actor_ptr sender,
                                         message_id mid, message msg,
                                         const forwarding_stack* fwd) {
  CAF_LOG_TRACE(CAF_ARG(id()) << CAF_ARG(sender)
                << CAF_ARG(mid) << CAF_ARG(msg));
  forwarding_stack tmp;
  shared_lock<detail::shared_spinlock> guard_(manager_mtx_);
  if (manager_)
    manager_->enqueue(nullptr, invalid_message_id,
                      make_message(forward_atom::value, std::move(sender),
                                   fwd ? *fwd : tmp,
                                   actor_cast<strong_actor_ptr>(this),
                                   mid, std::move(msg)),
                      nullptr);
}

void forwarding_actor_proxy::enqueue(mailbox_element_ptr what,
                                     execution_unit*) {
  CAF_PUSH_AID(0);
  if (! what)
    return;
  forward_msg(std::move(what->sender), what->mid,
              std::move(what->msg), &what->stages);
}


bool forwarding_actor_proxy::link_impl(linking_operation op,
                                       const actor_addr& other) {
  switch (op) {
    case establish_link_op:
      if (establish_link_impl(other)) {
        // causes remote actor to link to (proxy of) other
        // receiving peer will call: this->local_link_to(other)
        forward_msg(ctrl(), invalid_message_id,
                    make_message(link_atom::value, other));
        return true;
      }
      break;
    case remove_link_op:
      if (remove_link_impl(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(ctrl(), invalid_message_id,
                    make_message(unlink_atom::value, other));
        return true;
      }
      break;
    case establish_backlink_op:
      if (establish_backlink_impl(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(ctrl(), invalid_message_id,
                    make_message(link_atom::value, other));
        return true;
      }
      break;
    case remove_backlink_op:
      if (remove_backlink_impl(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(ctrl(), invalid_message_id,
                    make_message(unlink_atom::value, other));
        return true;
      }
      break;
  }
  return false;
}

void forwarding_actor_proxy::local_link_to(const actor_addr& other) {
  establish_link_impl(other);
}

void forwarding_actor_proxy::local_unlink_from(const actor_addr& other) {
  remove_link_impl(other);
}

void forwarding_actor_proxy::kill_proxy(execution_unit* ctx, exit_reason rsn) {
  CAF_ASSERT(ctx != nullptr);
  manager(invalid_actor);
  cleanup(rsn, ctx);
}

} // namespace caf
