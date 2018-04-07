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

#include "caf/config.hpp"
#include "caf/make_actor.hpp"
#include "caf/actor_companion.hpp"
#include "caf/message_handler.hpp"

#include "caf/scoped_execution_unit.hpp"

CAF_PUSH_WARNINGS
#include <QEvent>
#include <QApplication>
CAF_POP_WARNINGS

namespace caf {
namespace mixin {

template<typename Base, int EventId = static_cast<int>(QEvent::User + 31337)>
class actor_widget : public Base {
public:
  struct event_type : public QEvent {
    mailbox_element_ptr mptr;
    event_type(mailbox_element_ptr ptr)
        : QEvent(static_cast<QEvent::Type>(EventId)), mptr(std::move(ptr)) {
      // nop
    }
  };

  template <typename... Ts>
  actor_widget(Ts&&... xs) : Base(std::forward<Ts>(xs)...), alive_(false) {
    // nop
  }

  ~actor_widget() {
    if (companion_)
      self()->cleanup(error{}, &dummy_);
  }


  void init(actor_system& system) {
    alive_ = true;
    companion_ = actor_cast<strong_actor_ptr>(system.spawn<actor_companion>());
    self()->on_enqueue([=](mailbox_element_ptr ptr) {
      qApp->postEvent(this, new event_type(std::move(ptr)));
    });
    self()->on_exit([=] {
      // close widget if actor companion dies
      this->close();
    });
  }

  template <class F>
  void set_message_handler(F pfun) {
    self()->become(pfun(self()));
  }

  /// Terminates the actor companion and closes this widget.
  void quit_and_close(error exit_state = error{}) {
    self()->quit(std::move(exit_state));
    this->close();
  }

  bool event(QEvent* event) override {
    if (event->type() == static_cast<QEvent::Type>(EventId)) {
      auto ptr = dynamic_cast<event_type*>(event);
      if (ptr && alive_) {
        switch (self()->activate(&dummy_, *(ptr->mptr))) {
          default:
            break;
        };
        return true;
      }
    }
    return Base::event(event);
  }

  actor as_actor() const {
    CAF_ASSERT(companion_);
    return actor_cast<actor>(companion_);
  }

  actor_companion* self() {
    using bptr = abstract_actor*;  // base pointer
    using dptr = actor_companion*; // derived pointer
    return companion_ ? static_cast<dptr>(actor_cast<bptr>(companion_))
                      : nullptr;
  }

private:

  scoped_execution_unit dummy_;
  strong_actor_ptr companion_;
  bool alive_;
};

} // namespace mixin
} // namespace caf

