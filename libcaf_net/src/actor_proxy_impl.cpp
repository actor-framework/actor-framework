/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/net/actor_proxy_impl.hpp"

#include "caf/actor_system.hpp"
#include "caf/expected.hpp"
#include "caf/logger.hpp"

namespace caf {
namespace net {

actor_proxy_impl::actor_proxy_impl(actor_config& cfg, endpoint_manager_ptr dst)
  : super(cfg), sf_(dst->serialize_fun()), dst_(std::move(dst)) {
  // nop
}

actor_proxy_impl::~actor_proxy_impl() {
  // nop
}

void actor_proxy_impl::enqueue(mailbox_element_ptr what, execution_unit*) {
  CAF_PUSH_AID(0);
  CAF_ASSERT(what != nullptr);
  if (auto payload = sf_(home_system(), what->content()))
    dst_->enqueue(std::move(what), ctrl(), std::move(*payload));
  else
    CAF_LOG_ERROR(
      "unable to serialize payload: " << home_system().render(payload.error()));
}

bool actor_proxy_impl::add_backlink(abstract_actor* x) {
  if (monitorable_actor::add_backlink(x)) {
    enqueue(make_mailbox_element(ctrl(), make_message_id(), {},
                                 link_atom::value, x->ctrl()),
            nullptr);
    return true;
  }
  return false;
}

bool actor_proxy_impl::remove_backlink(abstract_actor* x) {
  if (monitorable_actor::remove_backlink(x)) {
    enqueue(make_mailbox_element(ctrl(), make_message_id(), {},
                                 unlink_atom::value, x->ctrl()),
            nullptr);
    return true;
  }
  return false;
}

void actor_proxy_impl::kill_proxy(execution_unit* ctx, error rsn) {
  cleanup(std::move(rsn), ctx);
}

} // namespace net
} // namespace caf
