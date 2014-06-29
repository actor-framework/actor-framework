/******************************************************************************\
 *                                                                            *
 *           ____                  _        _        _                        *
 *          | __ )  ___   ___  ___| |_     / \   ___| |_ ___  _ __            *
 *          |  _ \ / _ \ / _ \/ __| __|   / _ \ / __| __/ _ \| '__|           *
 *          | |_) | (_) | (_) \__ \ |_ _ / ___ \ (__| || (_) | |              *
 *          |____/ \___/ \___/|___/\__(_)_/   \_\___|\__\___/|_|              *
 *                                                                            *
 *                                                                            *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#include "cppa/send.hpp"
#include "cppa/to_string.hpp"

#include "cppa/io/middleman.hpp"
#include "cppa/io/remote_actor_proxy.hpp"

#include "cppa/detail/memory.hpp"
#include "cppa/detail/logging.hpp"
#include "cppa/detail/singletons.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

using namespace std;

namespace cppa {
namespace io {

inline sync_request_info* new_req_info(actor_addr sptr, message_id id) {
    return detail::memory::create<sync_request_info>(std::move(sptr), id);
}

sync_request_info::~sync_request_info() { }

sync_request_info::sync_request_info(actor_addr sptr, message_id id)
        : next(nullptr), sender(std::move(sptr)), mid(id) {
}

remote_actor_proxy::remote_actor_proxy(actor_id aid,
                                       node_id nid,
                                       actor parent)
        : super(aid, nid), m_parent(parent) {
    CPPA_REQUIRE(parent != invalid_actor);
    CPPA_LOG_INFO(CPPA_ARG(aid) << ", " << CPPA_TARG(nid, to_string));
}

remote_actor_proxy::~remote_actor_proxy() {
    anon_send(m_parent, make_message(atom("_DelProxy"),
                                     node(),
                                     m_id));
}

void remote_actor_proxy::forward_msg(const actor_addr& sender,
                                     message_id mid,
                                     message msg) {
    CPPA_LOG_TRACE(CPPA_ARG(m_id)
                   << ", " << CPPA_TSARG(sender)
                   << ", " << CPPA_MARG(mid, integer_value)
                   << ", " << CPPA_TSARG(msg));
    m_parent->enqueue(invalid_actor_addr,
                      message_id::invalid,
                      make_message(atom("_Dispatch"),
                                   sender,
                                   address(),
                                   mid,
                                   std::move(msg)),
                      nullptr);
}

void remote_actor_proxy::enqueue(const actor_addr& sender,
                                 message_id mid,
                                 message m,
                                 execution_unit*) {
    forward_msg(sender, mid, std::move(m));
}

void remote_actor_proxy::link_to(const actor_addr& other) {
    if (link_to_impl(other)) {
        // causes remote actor to link to (proxy of) other
        // receiving peer will call: this->local_link_to(other)
        forward_msg(address(), message_id::invalid,
                    make_message(atom("_Link"), other));
    }
}

void remote_actor_proxy::unlink_from(const actor_addr& other) {
    if (unlink_from_impl(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(address(), message_id::invalid,
                    make_message(atom("_Unlink"), other));
    }
}

bool remote_actor_proxy::establish_backlink(const actor_addr& other) {
    if (super::establish_backlink(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(address(), message_id::invalid,
                    make_message(atom("_Link"), other));
        return true;
    }
    return false;
}

bool remote_actor_proxy::remove_backlink(const actor_addr& other) {
    if (super::remove_backlink(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(address(), message_id::invalid,
                    make_message(atom("_Unlink"), other));
        return true;
    }
    return false;
}

void remote_actor_proxy::local_link_to(const actor_addr& other) {
    link_to_impl(other);
}

void remote_actor_proxy::local_unlink_from(const actor_addr& other) {
    unlink_from_impl(other);
}

void remote_actor_proxy::kill_proxy(uint32_t reason) {
    cleanup(reason);
}

} // namespace io
} // namespace cppa
