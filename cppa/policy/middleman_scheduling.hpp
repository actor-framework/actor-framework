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


#ifndef CPPA_POLICY_MIDDLEMAN_SCHEDULING_HPP
#define CPPA_POLICY_MIDDLEMAN_SCHEDULING_HPP

#include <utility>

#include "cppa/singletons.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/io/middleman.hpp"

#include "cppa/policy/cooperative_scheduling.hpp"

namespace cppa {
namespace policy {

// Actor must implement invoke_message
class middleman_scheduling {

 public:

    template<class Actor>
    class continuation {

     public:

        typedef intrusive_ptr<Actor> pointer;

        continuation(pointer ptr, msg_hdr_cref hdr, any_tuple&& msg)
        : m_self(std::move(ptr)), m_hdr(hdr), m_data(std::move(msg)) { }

        inline void operator()() const {
            m_self->invoke_message(m_hdr, std::move(m_data));
        }

     private:

        pointer        m_self;
        message_header m_hdr;
        any_tuple      m_data;

    };

    using timeout_type = int;

    // this does return nullptr
    template<class Actor, typename F>
    inline void fetch_messages(Actor*, F) {
        // clients cannot fetch messages
    }

    template<class Actor, typename F>
    inline void fetch_messages(Actor* self, F cb, timeout_type) {
        // a call to this call is always preceded by init_timeout,
        // which will trigger a timeout message
        fetch_messages(self, cb);
    }

    template<class Actor>
    inline void launch(Actor*) {
        // nothing to do
    }

    template<class Actor>
    void enqueue(Actor* self, msg_hdr_cref hdr, any_tuple& msg) {
        get_middleman()->run_later(continuation<Actor>{self, hdr, std::move(msg)});
    }

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_MIDDLEMAN_SCHEDULING_HPP
