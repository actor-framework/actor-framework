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
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
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


#include "cppa/cppa.hpp"
#include "cppa/singletons.hpp"
#include "cppa/detail/actor_registry.hpp"

#include "cppa/io/broker.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/broker_backend.hpp"

#include "cppa/detail/sync_request_bouncer.hpp"

namespace cppa { namespace io {

class broker_continuation {

 public:

    broker_continuation(broker_ptr ptr, mailbox_element* elem)
    : m_self(std::move(ptr)), m_elem(elem) { }

    inline void operator()() const {
        m_self->invoke_message(m_elem);
    }

 private:

    broker_ptr     m_self;
    mailbox_element* m_elem;

};

class default_broker_impl : public broker {

 public:

    typedef std::function<void (io_handle*)> function_type;

    default_broker_impl(function_type&& fun) : m_fun(std::move(fun)) { }

    void init() override {
        enqueue(nullptr, make_any_tuple(atom("INITMSG")));
        become(
            on(atom("INITMSG")) >> [=] {
                m_fun(&io());
            }
        );
    }

 private:

    function_type m_fun;

};

void broker::invoke_message(mailbox_element* elem) {
    if (exit_reason() != exit_reason::not_exited) {
        if (elem->mid.valid()) {
            detail::sync_request_bouncer srb{exit_reason()};
            srb(*elem);
            detail::disposer d;
            d(elem);
        }
        return;
    }
    try {
        scoped_self_setter sss{this};
        m_bhvr_stack.invoke(m_recv_policy, this, elem);
    }
    catch (std::exception& e) {
        CPPA_LOG_ERROR("IO actor killed due to an unhandled exception: "
                       << to_verbose_string(e));
        // keep compiler happy in non-debug mode
        static_cast<void>(e);
        quit(exit_reason::unhandled_exception);
    }
    catch (...) {
        CPPA_LOG_ERROR("IO actor killed due to an unhandled exception");
        quit(exit_reason::unhandled_exception);
    }
    if (exit_reason() != exit_reason::not_exited) {
        get_actor_registry()->dec_running();
        m_parent.reset();
    }
}

void broker::invoke_message(any_tuple msg) {
    invoke_message(mailbox_element::create(this, std::move(msg)));
}

void broker::enqueue(const message_header& hdr, any_tuple msg) {
    auto e = mailbox_element::create(hdr, std::move(msg));
    get_middleman()->run_later(broker_continuation{this, e});
}

bool broker::initialized() const {
    return true;
}

void broker::quit(std::uint32_t reason) {
    cleanup(reason);
    m_parent->handle_disconnect();
}

io_handle& broker::io() {
    return *m_parent;
}

intrusive_ptr<broker> broker::from(std::function<void (io_handle*)> fun) {
    return make_counted<default_broker_impl>(std::move(fun));
}

} } // namespace cppa::network
