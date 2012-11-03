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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_ACTOR_WIDGET_MIXIN_HPP
#define CPPA_ACTOR_WIDGET_MIXIN_HPP

#include <QEvent>
#include <QApplication>

#include "cppa/actor_companion_mixin.hpp"

namespace cppa {

template<typename Base, int EventId = static_cast<int>(QEvent::User + 31337)>
class actor_widget_mixin : public actor_companion_mixin<Base> {

    typedef actor_companion_mixin<Base> super;

    typedef typename super::message_pointer message_pointer;

 public:

    struct event_type : public QEvent {

        message_pointer msg;

        event_type(message_pointer mptr)
        : QEvent(static_cast<QEvent::Type>(EventId)), msg(std::move(mptr)) { }

    };

    virtual bool event(QEvent* event) {
        if (event->type() == static_cast<QEvent::Type>(EventId)) {
            auto ptr = dynamic_cast<event_type*>(event);
            if (ptr) {
                this->handle_message(ptr->msg);
                return true;
            }
        }
        return super::event(event);
    }

 protected:

    virtual void new_message(message_pointer ptr) {
        qApp->postEvent(this, new event_type(std::move(ptr)));
    }

};

} // namespace cppa

#endif // CPPA_ACTOR_WIDGET_MIXIN_HPP
