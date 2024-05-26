// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"
#include "caf/detail/core_export.hpp"

namespace caf {

class CAF_CORE_EXPORT default_attachable : public attachable {
public:
  enum observe_type { monitor, link };

  struct observe_token {
    actor_addr observer;
    observe_type type;
    static constexpr size_t token_type = attachable::token::observer;
  };

  void actor_exited(const error& rsn, scheduler* sched) override;

  bool matches(const token& what) override;

  static attachable_ptr
  make_monitor(actor_addr observed, actor_addr observer,
               message_priority prio = message_priority::normal) {
    return attachable_ptr{new default_attachable(
      std::move(observed), std::move(observer), monitor, prio)};
  }

  static attachable_ptr make_link(actor_addr observed, actor_addr observer) {
    return attachable_ptr{
      new default_attachable(std::move(observed), std::move(observer), link)};
  }

  class predicate {
  public:
    predicate(actor_addr observer, observe_type type)
      : observer_(std::move(observer)), type_(type) {
      // nop
    }

    bool operator()(const attachable_ptr& ptr) const {
      return ptr->matches(observe_token{observer_, type_});
    }

  private:
    actor_addr observer_;
    observe_type type_;
  };

private:
  default_attachable(actor_addr observed, actor_addr observer,
                     observe_type type,
                     message_priority prio = message_priority::normal);

  /// Holds a weak reference to the observed actor.
  actor_addr observed_;

  /// Holds a weak reference to the observing actor.
  actor_addr observer_;

  /// Defines the type of message we wish to send.
  observe_type type_;

  /// Defines the priority for the message.
  message_priority priority_;
};

} // namespace caf
