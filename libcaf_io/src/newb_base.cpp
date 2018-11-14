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

#include "caf/io/network/newb_base.hpp"

namespace caf {
namespace io {
namespace network {

newb_base::newb_base(actor_config& cfg, network::default_multiplexer& dm,
                     network::native_socket sockfd)
    : super(cfg),
      event_handler(dm, sockfd) {
  // nop
}

void newb_base::enqueue(mailbox_element_ptr ptr, execution_unit*) {
  CAF_PUSH_AID(this->id());
  scheduled_actor::enqueue(std::move(ptr), &backend());
}

void newb_base::enqueue(strong_actor_ptr src, message_id mid, message msg,
                        execution_unit*) {
  enqueue(make_mailbox_element(std::move(src), mid, {}, std::move(msg)),
          &backend());
}

resumable::subtype_t newb_base::subtype() const {
  return resumable::io_actor;
}

const char* newb_base::name() const {
  return "newb";
}

void newb_base::launch(execution_unit* eu, bool lazy, bool hide) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_ASSERT(eu != nullptr);
  CAF_ASSERT(eu == &backend());
  CAF_LOG_TRACE(CAF_ARG(lazy) << CAF_ARG(hide));
  // Add implicit reference count held by middleman/multiplexer.
  if (!hide)
    super::register_at_system();
  if (lazy && super::mailbox().try_block())
    return;
  intrusive_ptr_add_ref(super::ctrl());
  eu->exec_later(this);
}

void newb_base::initialize() {
  CAF_LOG_TRACE("");
  init_newb();
  auto bhvr = make_behavior();
  CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                   << CAF_ARG(this->has_behavior()));
  if (bhvr) {
    // make_behavior() did return a behavior instead of using become().
    CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
    this->become(std::move(bhvr));
  }
}

bool newb_base::cleanup(error&& reason, execution_unit* host) {
  CAF_LOG_TRACE(CAF_ARG(reason));
  stop();
  // TODO: Should policies be notified here?
  return local_actor::cleanup(std::move(reason), host);
}

network::multiplexer::runnable::resume_result
newb_base::resume(execution_unit* ctx, size_t mt) {
  CAF_PUSH_AID_FROM_PTR(this);
  CAF_ASSERT(ctx != nullptr);
  CAF_ASSERT(ctx == &backend());
  return scheduled_actor::resume(ctx, mt);
}

void newb_base::init_newb() {
  CAF_LOG_TRACE("");
  this->setf(super::is_initialized_flag);
}

behavior newb_base::make_behavior() {
  behavior res;
  if (this->initial_behavior_fac_) {
    res = this->initial_behavior_fac_(this);
    this->initial_behavior_fac_ = nullptr;
  }
  return res;
}

} // namespace network
} // namespace io
} // namespace caf
