/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#include <utility>

#include "caf/forwarding_actor_proxy.hpp"

#include "caf/send.hpp"
#include "caf/locks.hpp"
#include "caf/logger.hpp"
#include "caf/stream_msg.hpp"
#include "caf/mailbox_element.hpp"

namespace caf {

forwarding_actor_proxy::forwarding_actor_proxy(actor_config& cfg, actor dest,
                                               actor stream_serv)
    : actor_proxy(cfg),
      broker_(std::move(dest)),
      stream_serv_(std::move(stream_serv)) {
  // nop
}

forwarding_actor_proxy::~forwarding_actor_proxy() {
  anon_send(broker_, make_message(delete_atom::value, node(), id()));
}

void forwarding_actor_proxy::forward_msg(strong_actor_ptr sender,
                                         message_id mid, message msg,
                                         const forwarding_stack* fwd) {
  CAF_LOG_TRACE(CAF_ARG(id()) << CAF_ARG(sender)
                << CAF_ARG(mid) << CAF_ARG(msg));
  forwarding_stack tmp;
  shared_lock<detail::shared_spinlock> guard(mtx_);
  if (broker_)
    broker_->enqueue(nullptr, invalid_message_id,
                     make_message(forward_atom::value, std::move(sender),
                                  fwd != nullptr ? *fwd : tmp,
                                  strong_actor_ptr{ctrl()}, mid,
                                  std::move(msg)),
                     nullptr);
}

void forwarding_actor_proxy::enqueue(mailbox_element_ptr what,
                                     execution_unit* context) {
  CAF_PUSH_AID(0);
  CAF_ASSERT(what);
  if (what->content().type_token() != make_type_token<stream_msg>()) {
    forward_msg(std::move(what->sender), what->mid,
                what->move_content_to_message(), &what->stages);
  } else {
    shared_lock<detail::shared_spinlock> guard(mtx_);
    if (stream_serv_) {
      // Push this actor the the forwarding stack and move the message
      // to the stream_serv, which will intercept stream handshakes.
      // Since the stream_serv becomes part of the pipeline, the proxy
      // will never receive a stream_msg unless it is the initial handshake.
      what->stages.emplace_back(ctrl());
      auto msg = what->move_content_to_message();
      auto prefix = make_message(sys_atom::value);
      stream_serv_->enqueue(make_mailbox_element(std::move(what->sender),
                                                 what->mid,
                                                 std::move(what->stages),
                                                 prefix + msg),
                            context);
      //what->stages.emplace_back(ctrl());
      //stream_serv_->enqueue(std::move(what), context);
    }
  }
}

bool forwarding_actor_proxy::add_backlink(abstract_actor* x) {
  if (monitorable_actor::add_backlink(x)) {
    forward_msg(ctrl(), invalid_message_id,
                make_message(link_atom::value, x->ctrl()));
    return true;
  }
  return false;
}

bool forwarding_actor_proxy::remove_backlink(abstract_actor* x) {
  if (monitorable_actor::remove_backlink(x)) {
    forward_msg(ctrl(), invalid_message_id,
                make_message(unlink_atom::value, x->ctrl()));
    return true;
  }
  return false;
}

void forwarding_actor_proxy::kill_proxy(execution_unit* ctx, error rsn) {
  CAF_ASSERT(ctx != nullptr);
  actor tmp[2];
  { // lifetime scope of guard
    std::unique_lock<detail::shared_spinlock> guard(mtx_);
    broker_.swap(tmp[0]); // manually break cycle
    stream_serv_.swap(tmp[1]); // manually break cycle
  }
  cleanup(std::move(rsn), ctx);
}

} // namespace caf
