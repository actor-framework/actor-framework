// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/abstract_broker.hpp"
#include "caf/io/fwd.hpp"
#include "caf/io/system_messages.hpp"

#include "caf/fwd.hpp"
#include "caf/mailbox_element.hpp"
#include "caf/proxy_registry.hpp"

namespace caf::io {

/// Base class for `scribe` and `doorman`.
/// @ingroup Broker
template <class Base, class Handle, class SysMsgType>
class broker_servant : public Base {
public:
  using handle_type = Handle;

  broker_servant(handle_type x)
    : hdl_(x),
      value_(strong_actor_ptr{}, make_message_id(),
             make_message(SysMsgType{x, {}})) {
    // nop
  }

  handle_type hdl() const {
    return hdl_;
  }

  void halt() {
    activity_tokens_ = std::nullopt;
    this->remove_from_loop();
  }

  void trigger() {
    activity_tokens_ = std::nullopt;
    this->add_to_loop();
  }

  void trigger(size_t num) {
    CAF_ASSERT(num > 0);
    if (activity_tokens_)
      *activity_tokens_ += num;
    else
      activity_tokens_ = num;
    this->add_to_loop();
  }

  std::optional<size_t> activity_tokens() const {
    return activity_tokens_;
  }

protected:
  void detach_from(abstract_broker* ptr) override {
    ptr->erase(hdl_);
  }

  void invoke_mailbox_element_impl(execution_unit* ctx, mailbox_element& x) {
    auto self = this->parent();
    if (auto pfac = self->proxy_registry_ptr()) {
      proxy_registry::current(pfac);
      auto guard = detail::scope_guard{[]() noexcept { //
        proxy_registry::current(nullptr);
      }};
      self->activate(ctx, x);
    } else {
      self->activate(ctx, x);
    }
  }

  bool invoke_mailbox_element(execution_unit* ctx) {
    // hold on to a strong reference while "messing" with the parent actor
    strong_actor_ptr ptr_guard{this->parent()->ctrl()};
    auto prev = activity_tokens_;
    invoke_mailbox_element_impl(ctx, value_);
    // only consume an activity token if actor did not produce them now
    if (prev && activity_tokens_ && --(*activity_tokens_) == 0) {
      if (this->parent()->getf(abstract_actor::is_shutting_down_flag
                               | abstract_actor::is_terminated_flag))
        return false;
      // tell broker it entered passive mode, this can result in
      // producing, why we check the condition again afterwards
      using passive_t = std::conditional_t<
        std::is_same_v<handle_type, connection_handle>,
        connection_passivated_msg,
        std::conditional_t<std::is_same_v<handle_type, accept_handle>,
                           acceptor_passivated_msg,
                           datagram_servant_passivated_msg>>;
      mailbox_element tmp{strong_actor_ptr{}, make_message_id(),
                          make_message(passive_t{hdl()})};
      invoke_mailbox_element_impl(ctx, tmp);
      return activity_tokens_ != size_t{0};
    }
    return true;
  }

  SysMsgType& msg() {
    return value_.payload.template get_mutable_as<SysMsgType>(0);
  }

  handle_type hdl_;
  mailbox_element value_;
  std::optional<size_t> activity_tokens_;
};

} // namespace caf::io
