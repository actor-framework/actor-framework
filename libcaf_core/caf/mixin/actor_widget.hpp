/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcaf.                                              *
 * libcaf is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcaf is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcaf. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_ACTOR_WIDGET_MIXIN_HPP
#define CPPA_ACTOR_WIDGET_MIXIN_HPP

#include "caf/config.hpp"
#include "caf/make_counted.hpp"
#include "caf/actor_companion.hpp"
#include "caf/message_handler.hpp"

CAF_PUSH_WARNINGS
#include <QEvent>
#include <QApplication>
CAF_POP_WARNINGS

namespace caf {
namespace mixin {

template<typename Base, int EventId = static_cast<int>(QEvent::User + 31337)>
class actor_widget : public Base {
public:
  typedef typename actor_companion::message_pointer message_pointer;

  struct event_type : public QEvent {
    message_pointer mptr;
    event_type(message_pointer ptr)
        : QEvent(static_cast<QEvent::Type>(EventId)), mptr(std::move(ptr)) {
      // nop
    }
  };

  template <typename... Ts>
  actor_widget(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    companion_ = make_counted<actor_companion>();
    companion_->on_enqueue([=](message_pointer ptr) {
      qApp->postEvent(this, new event_type(std::move(ptr)));
    });
  }

  template <typename T>
  void set_message_handler(T pfun) {
    companion_->become(pfun(companion_.get()));
  }

  bool event(QEvent* event) override {
    if (event->type() == static_cast<QEvent::Type>(EventId)) {
      auto ptr = dynamic_cast<event_type*>(event);
      if (ptr) {
        companion_->invoke_message(ptr->mptr,
                                    companion_->get_behavior(),
                                    companion_->awaited_response_id());
        return true;
      }
    }
    return Base::event(event);
  }

  actor as_actor() const {
    return companion_;
  }

private:
    actor_companion_ptr companion_;
};

} // namespace mixin
} // namespace caf

#endif // CPPA_ACTOR_WIDGET_MIXIN_HPP
