// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/forwarding_actor_proxy.hpp"

#include "caf/logger.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/send.hpp"

#include <utility>

namespace caf {

forwarding_actor_proxy::forwarding_actor_proxy(actor_config& cfg, actor dest)
  : actor_proxy(cfg), broker_(std::move(dest)) {
  anon_send(broker_, monitor_atom_v, ctrl());
}

forwarding_actor_proxy::~forwarding_actor_proxy() {
  anon_send(broker_, make_message(delete_atom_v, node(), id()));
}

const char* forwarding_actor_proxy::name() const {
  return "caf.forwarding-actor-proxy";
}

bool forwarding_actor_proxy::forward_msg(strong_actor_ptr sender,
                                         message_id mid, message msg) {
  CAF_LOG_TRACE(CAF_ARG(id())
                << CAF_ARG(sender) << CAF_ARG(mid) << CAF_ARG(msg));
  if (msg.match_elements<exit_msg>())
    unlink_from(msg.get_as<exit_msg>(0).source);
  auto ptr = make_mailbox_element(nullptr, make_message_id(), forward_atom_v,
                                  std::move(sender), strong_actor_ptr{ctrl()},
                                  mid, std::move(msg));
  std::shared_lock guard{broker_mtx_};
  if (broker_)
    return broker_->enqueue(std::move(ptr), nullptr);
  return false;
}

bool forwarding_actor_proxy::enqueue(mailbox_element_ptr what,
                                     execution_unit*) {
  CAF_PUSH_AID(0);
  CAF_ASSERT(what);
  return forward_msg(std::move(what->sender), what->mid,
                     std::move(what->payload));
}

bool forwarding_actor_proxy::add_backlink(abstract_actor* x) {
  if (abstract_actor::add_backlink(x)) {
    forward_msg(ctrl(), make_message_id(),
                make_message(link_atom_v, x->ctrl()));
    return true;
  }
  return false;
}

bool forwarding_actor_proxy::remove_backlink(abstract_actor* x) {
  if (abstract_actor::remove_backlink(x)) {
    forward_msg(ctrl(), make_message_id(),
                make_message(unlink_atom_v, x->ctrl()));
    return true;
  }
  return false;
}

void forwarding_actor_proxy::kill_proxy(execution_unit* ctx, error rsn) {
  actor tmp;
  { // lifetime scope of guard
    std::unique_lock guard{broker_mtx_};
    broker_.swap(tmp); // manually break cycle
  }
  cleanup(std::move(rsn), ctx);
}

} // namespace caf
