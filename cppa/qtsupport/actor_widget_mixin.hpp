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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_QTSUPPORT_ACTOR_WIDGET_MIXIN_HPP
#define CPPA_QTSUPPORT_ACTOR_WIDGET_MIXIN_HPP

#include <QEvent>
#include <QApplication>

#include "cppa/actor_companion.hpp"
#include "cppa/partial_function.hpp"

#include "cppa/policy/sequential_invoke.hpp"

namespace cppa {

template<typename Base, int EventId = static_cast<int>(QEvent::User + 31337)>
class actor_widget_mixin : public Base {

 public:

    typedef typename actor_companion::message_pointer message_pointer;

    struct event_type : public QEvent {

        message_pointer mptr;

        event_type(message_pointer ptr)
        : QEvent(static_cast<QEvent::Type>(EventId)), mptr(std::move(ptr)) { }

    };

    template<typename... Ts>
    actor_widget_mixin(Ts&&... args) : Base(std::forward<Ts>(args)...) {
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

} // namespace cppa

#endif // CPPA_QTSUPPORT_ACTOR_WIDGET_MIXIN_HPP
