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

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/attachable.hpp"

namespace caf {

class default_attachable : public attachable {
public:
  enum observe_type {
    monitor,
    link
  };

  struct observe_token {
    actor_addr observer;
    observe_type type;
    static constexpr size_t token_type = attachable::token::observer;
  };

  void actor_exited(const error& rsn, execution_unit* host) override;

  bool matches(const token& what) override;

  static attachable_ptr
  make_monitor(actor_addr observed, actor_addr observer,
               message_priority prio = message_priority::normal) {
    return attachable_ptr{new default_attachable(
      std::move(observed), std::move(observer), monitor, prio)};
  }

  static attachable_ptr make_link(actor_addr observed, actor_addr observer) {
    return attachable_ptr{new default_attachable(std::move(observed),
                                                 std::move(observer), link)};
  }

  class predicate {
  public:
    inline predicate(actor_addr observer, observe_type type)
        : observer_(std::move(observer)),
          type_(type) {
      // nop
    }

    inline bool operator()(const attachable_ptr& ptr) const {
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


