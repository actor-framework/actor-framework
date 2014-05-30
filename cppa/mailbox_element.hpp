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


#ifndef CPPA_MAILBOX_ELEMENT_HPP
#define CPPA_MAILBOX_ELEMENT_HPP

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

    ~mailbox_element();

    mailbox_element(mailbox_element&&) = delete;
    mailbox_element(const mailbox_element&) = delete;
    mailbox_element& operator=(mailbox_element&&) = delete;
    mailbox_element& operator=(const mailbox_element&) = delete;

    template<typename T>
    inline static mailbox_element* create(msg_hdr_cref hdr, T&& data) {
        return detail::memory::create<mailbox_element>(hdr, std::forward<T>(data));
    }

 private:

    mailbox_element() = default;

    mailbox_element(msg_hdr_cref hdr, any_tuple data);

};

typedef std::unique_ptr<mailbox_element, detail::disposer>
        unique_mailbox_element_pointer;

} // namespace cppa

#endif // CPPA_MAILBOX_ELEMENT_HPP
