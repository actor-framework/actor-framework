// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/local_actor.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/default_attachable.hpp"
#include "caf/disposable.hpp"
#include "caf/exit_reason.hpp"
#include "caf/log/core.hpp"
#include "caf/logger.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/resumable.hpp"
#include "caf/scheduler.hpp"
#include "caf/sec.hpp"
#include "caf/telemetry/histogram.hpp"
#include "caf/telemetry/metric.hpp"
#include "caf/telemetry/metric_family.hpp"
#include "caf/telemetry/metric_family_impl.hpp"

#include <algorithm>
#include <condition_variable>
#include <string>

namespace caf {

local_actor::local_actor(actor_config& cfg)
  : abstract_actor(cfg),
    context_(cfg.sched),
    current_element_(nullptr),
    initial_behavior_fac_(std::move(cfg.init_fun)) {
  // nop
}

local_actor::~local_actor() {
  // nop
}

void local_actor::setup_metrics() {
  metrics_ = system().make_actor_metrics(name());
  if (auto* running_count = metrics_.running_count) {
    running_count->inc();
  }
}

auto local_actor::now() const noexcept -> clock_type::time_point {
  return clock().now();
}

disposable local_actor::request_response_timeout(timespan timeout,
                                                 message_id mid) {
  auto lg = log::core::trace("timeout = {}, mid = {}", timeout, mid);
  if (timeout == infinite)
    return {};
  auto t = clock().now() + timeout;
  return clock().schedule_message(
    t, strong_actor_ptr{ctrl(), add_ref},
    make_mailbox_element(nullptr, mid.response_id(),
                         make_error(sec::request_timeout)));
}

void local_actor::monitor(const node_id& node) {
  system().monitor(node, address());
}

void local_actor::demonitor(const node_id& node) {
  system().demonitor(node, address());
}

void local_actor::do_monitor(abstract_actor* ptr, message_priority priority) {
  if (ptr != nullptr)
    ptr->attach(
      default_attachable::make_monitor(ptr->address(), address(), priority));
}

void local_actor::do_demonitor(const strong_actor_ptr& whom) {
  auto lg = log::core::trace("whom = {}", whom);
  if (whom) {
    default_attachable::observe_token tk{address(),
                                         default_attachable::monitor};
    whom->get()->detach(tk);
  }
}

void local_actor::on_exit() {
  // nop
}

message_id local_actor::new_request_id(message_priority mp) noexcept {
  auto result = ++last_request_id_;
  return mp == message_priority::normal ? result : result.with_high_priority();
}

uint64_t local_actor::new_u64_id() noexcept {
  auto result = ++last_request_id_;
  return result.integer_value();
}

void local_actor::send_exit(const actor_addr& whom, error reason) {
  send_exit(actor_cast<strong_actor_ptr>(whom), std::move(reason));
}

void local_actor::send_exit(const strong_actor_ptr& receiver, error reason) {
  if (receiver)
    receiver->enqueue(make_mailbox_element(nullptr, make_message_id(),
                                           exit_msg{address(),
                                                    std::move(reason)}),
                      context());
}

const char* local_actor::name() const {
  return "user.local-actor";
}

void local_actor::do_delegate_error() {
  auto& sender = current_element_->sender;
  auto& mid = current_element_->mid;
  if (!sender || mid.is_response() || mid.is_answered())
    return;
  sender->enqueue(make_mailbox_element({ctrl(), add_ref}, mid.response_id(),
                                       make_error(sec::invalid_delegate)),
                  context());
  mid.mark_as_answered();
}

void local_actor::on_cleanup([[maybe_unused]] const error& reason) {
  auto lg = log::core::trace("reason = {}", reason);
  if (auto* running_count = metrics_.running_count) {
    running_count->dec();
  }
  on_exit();
  CAF_LOG_TERMINATE_EVENT(this, reason);
}

// -- send functions -----------------------------------------------------------

void local_actor::do_anon_send(abstract_actor* receiver,
                               message_priority priority, message&& msg) {
  if (receiver != nullptr) {
    receiver->enqueue(make_mailbox_element(nullptr, make_message_id(priority),
                                           std::move(msg)),
                      context());
    return;
  }
  system().message_rejected(nullptr);
}

disposable local_actor::do_scheduled_anon_send(strong_actor_ptr receiver,
                                               message_priority priority,
                                               actor_clock::time_point timeout,
                                               message&& msg) {
  if (receiver != nullptr) {
    return clock().schedule_message(
      timeout, receiver,
      make_mailbox_element(nullptr, make_message_id(priority), std::move(msg)));
  }
  system().message_rejected(nullptr);
  return {};
}

// -- miscellaneous ------------------------------------------------------------

resumable* local_actor::as_resumable() noexcept {
  return nullptr;
}

} // namespace caf
