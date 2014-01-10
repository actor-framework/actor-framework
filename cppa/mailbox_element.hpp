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


#ifndef CPPA_RECURSIVE_QUEUE_NODE_HPP
#define CPPA_RECURSIVE_QUEUE_NODE_HPP

#include <cstdint>

#include "cppa/extend.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/actor_addr.hpp"
#include "cppa/message_id.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/memory_cached.hpp"
#include "cppa/message_header.hpp"

// needs access to constructor + destructor to initialize m_dummy_node
namespace cppa {

class local_actor;

class mailbox_element : public extend<memory_managed>::with<memory_cached> {

    friend class local_actor;
    friend class detail::memory;

 public:

    mailbox_element* next;   // intrusive next pointer
    bool             marked; // denotes if this node is currently processed
    actor_addr       sender;
    any_tuple        msg;    // 'content field'
    message_id       mid;

    mailbox_element(mailbox_element&&) = delete;
    mailbox_element(const mailbox_element&) = delete;
    mailbox_element& operator=(mailbox_element&&) = delete;
    mailbox_element& operator=(const mailbox_element&) = delete;

    template<typename T>
    inline static mailbox_element* create(const message_header& hdr, T&& data) {
        return detail::memory::create<mailbox_element>(hdr, std::forward<T>(data));
    }

 private:

    mailbox_element() = default;

    mailbox_element(const message_header& hdr, any_tuple data);

};

typedef std::unique_ptr<mailbox_element, detail::disposer>
        unique_mailbox_element_pointer;

} // namespace cppa

#endif // CPPA_RECURSIVE_QUEUE_NODE_HPP
