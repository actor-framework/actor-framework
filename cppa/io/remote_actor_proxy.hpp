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

#ifndef CPPA_IO_REMOTE_ACTOR_PROXY_HPP
#define CPPA_IO_REMOTE_ACTOR_PROXY_HPP

#include "cppa/extend.hpp"
#include "cppa/actor_proxy.hpp"

#include "cppa/mixin/memory_cached.hpp"

#include "cppa/detail/single_reader_queue.hpp"

namespace cppa {
namespace detail {

class memory;

} // namespace detail
} // namespace cppa

namespace cppa {
namespace io {

class middleman;

class sync_request_info : public extend<memory_managed>::
                                 with<mixin::memory_cached> {

    friend class detail::memory;

 public:

    using pointer = sync_request_info*;

    ~sync_request_info();

    pointer next;             // intrusive next pointer
    actor_addr sender; // points to the sender of the message
    message_id mid;    // sync message ID

 private:

    sync_request_info(actor_addr sptr, message_id id);

};

class remote_actor_proxy : public actor_proxy {

    using super = actor_proxy;

 public:

    remote_actor_proxy(actor_id mid, node_id pinfo,
                       actor parent);

    void enqueue(const actor_addr&, message_id, message,
                 execution_unit*) override;

    void link_to(const actor_addr& other) override;

    void unlink_from(const actor_addr& other) override;

    bool remove_backlink(const actor_addr& to) override;

    bool establish_backlink(const actor_addr& to) override;

    void local_link_to(const actor_addr& other) override;

    void local_unlink_from(const actor_addr& other) override;

    void kill_proxy(uint32_t reason) override;

 protected:

    ~remote_actor_proxy();

 private:

    void forward_msg(const actor_addr& sender, message_id mid,
                     message msg);

    actor m_parent;

};

using remote_actor_proxy_ptr = intrusive_ptr<remote_actor_proxy>;

} // namespace io
} // namespace cppa

#endif // CPPA_IO_REMOTE_ACTOR_PROXY_HPP
