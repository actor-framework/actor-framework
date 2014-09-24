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

#include <QEvent>
#include <QApplication>

#include "caf/actor_companion.hpp"
#include "caf/message_handler.hpp"

#include "caf/policy/sequential_invoke.hpp"

namespace caf {
namespace mixin {
        
template<typename Base, int EventId = static_cast<int>(QEvent::User + 31337)>
class actor_widget : public Base {

 public:

    typedef typename actor_companion::message_pointer message_pointer;

    struct event_type : public QEvent {

        message_pointer mptr;

        event_type(message_pointer ptr)
        : QEvent(static_cast<QEvent::Type>(EventId)), mptr(std::move(ptr)) { }

    };

    template<typename... Ts>
    actor_widget(Ts&&... args) : Base(std::forward<Ts>(args)...) {
        m_companion.reset(detail::memory::create<actor_companion>());
        m_companion->on_enqueue([=](message_pointer ptr) {
            qApp->postEvent(this, new event_type(std::move(ptr)));
        });
    }

    template<typename T>
    void set_message_handler(T pfun) {
        m_companion->become(pfun(m_companion.get()));
    }

    virtual bool event(QEvent* event) {
        if (event->type() == static_cast<QEvent::Type>(EventId)) {
            auto ptr = dynamic_cast<event_type*>(event);
            if (ptr) {
                m_invoke.handle_message(m_companion.get(),
                                        ptr->mptr.release(),
                                        m_companion->bhvr_stack().back(),
                                        m_companion->bhvr_stack().back_id());
                return true;
            }
        }
        return Base::event(event);
    }

    actor as_actor() const {
        return m_companion;
    }

 private:

    policy::sequential_invoke m_invoke;
    actor_companion_ptr m_companion;

};

} // namespace mixin
} // namespace caf

#endif // CPPA_ACTOR_WIDGET_MIXIN_HPP
