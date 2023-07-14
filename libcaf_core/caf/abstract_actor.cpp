// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/config.hpp"

#include <atomic>
#include <map>
#include <mutex>
#include <stdexcept>

#include "caf/abstract_actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/actor_cast.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/actor_registry.hpp"
#include "caf/actor_system.hpp"
#include "caf/config.hpp"
#include "caf/default_attachable.hpp"
#include "caf/execution_unit.hpp"
#include "caf/logger.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/message.hpp"
#include "caf/system_messages.hpp"

namespace caf {

// exit_state_ is guaranteed to be set to 0, i.e., exit_reason::not_exited,
// by std::atomic<> constructor

actor_control_block* abstract_actor::ctrl() const {
  return actor_control_block::from(this);
}

abstract_actor::~abstract_actor() {
  // nop
}

void abstract_actor::on_destroy() {
  // nop
}

bool abstract_actor::enqueue(strong_actor_ptr sender, message_id mid,
                             message msg, execution_unit* host) {
  return enqueue(make_mailbox_element(sender, mid, {}, std::move(msg)), host);
}

abstract_actor::abstract_actor(actor_config& cfg)
  : abstract_channel(cfg.flags) {
  // nop
}

actor_addr abstract_actor::address() const noexcept {
  return actor_addr{actor_control_block::from(this)};
}

std::set<std::string> abstract_actor::message_types() const {
  // defaults to untyped
  return std::set<std::string>{};
}

actor_id abstract_actor::id() const noexcept {
  return actor_control_block::from(this)->id();
}

node_id abstract_actor::node() const noexcept {
  return actor_control_block::from(this)->node();
}

actor_system& abstract_actor::home_system() const noexcept {
  return *(actor_control_block::from(this)->home_system);
}

mailbox_element* abstract_actor::peek_at_next_mailbox_element() {
  return nullptr;
}

void abstract_actor::register_at_system() {
  if (getf(is_registered_flag))
    return;
  setf(is_registered_flag);
  [[maybe_unused]] auto count = home_system().registry().inc_running();
  CAF_LOG_DEBUG("actor" << id() << "increased running count to" << count);
}

void abstract_actor::unregister_from_system() {
  if (!getf(is_registered_flag))
    return;
  unsetf(is_registered_flag);
  [[maybe_unused]] auto count = home_system().registry().dec_running();
  CAF_LOG_DEBUG("actor" << id() << "decreased running count to" << count);
}

} // namespace caf
