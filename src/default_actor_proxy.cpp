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


#include "cppa/to_string.hpp"

#include "cppa/logging.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/default_actor_proxy.hpp"

#include "cppa/detail/memory.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

using namespace std;

namespace cppa { namespace io {

inline sync_request_info* new_req_info(actor_ptr sptr, message_id id) {
    return detail::memory::create<sync_request_info>(std::move(sptr), id);
}

sync_request_info::sync_request_info(actor_ptr sptr, message_id id)
: next(nullptr), sender(std::move(sptr)), mid(id) { }

default_actor_proxy::default_actor_proxy(actor_id mid,
                                         const process_information_ptr& pinfo,
                                         default_protocol* parent)
: super(mid), m_parent(parent), m_pinf(pinfo) {
    CPPA_REQUIRE(parent != nullptr);
    CPPA_LOG_INFO(CPPA_ARG(mid) << ", " << CPPA_TARG(pinfo, to_string)
                  << "protocol = " << detail::demangle(typeid(*parent)));
}

default_actor_proxy::~default_actor_proxy() {
    auto aid = m_id;
    auto node = m_pinf;
    auto proto = m_parent;
    CPPA_LOG_INFO(CPPA_ARG(m_id) << ", " << CPPA_TSARG(m_pinf)
                   << ", protocol = " << detail::demangle(typeid(*m_parent)));
    proto->run_later([aid, node, proto] {
        CPPA_LOGC_TRACE("cppa::io::default_actor_proxy",
                        "~default_actor_proxy$run_later",
                        "node = " << to_string(*node) << ", aid " << aid
                        << ", proto = " << to_string(proto->identifier()));
        proto->addressing()->erase(*node, aid);
        auto p = proto->get_peer(*node);
        if (p && p->erase_on_last_proxy_exited()) {
            if (proto->addressing()->count_proxies(*node) == 0) {
                proto->last_proxy_exited(p);
            }
        }
    });
}

void default_actor_proxy::deliver(const message_header& hdr, any_tuple msg) {
    // this member function is exclusively called from default_peer from inside
    // the middleman's thread, therefore we can safely access
    // m_pending_requests here
    if (hdr.id.is_response()) {
        // remove this request from list of pending requests
        auto req = hdr.id.request_id();
        m_pending_requests.remove_if([&](const sync_request_info& e) -> bool {
            return e.mid == req;
        });
    }
    hdr.deliver(std::move(msg));
}

void default_actor_proxy::forward_msg(const message_header& hdr, any_tuple msg) {
    CPPA_LOG_TRACE(CPPA_ARG(m_id) << ", " << CPPA_TSARG(hdr)
                   << ", " << CPPA_TSARG(msg));
    if (hdr.receiver != this) {
        auto cpy = hdr;
        cpy.receiver = this;
        forward_msg(cpy, std::move(msg));
        return;
    }
    if (hdr.sender && hdr.id.is_request()) {
        switch (m_pending_requests.enqueue(new_req_info(hdr.sender, hdr.id))) {
            case intrusive::queue_closed: {
                auto rsn = exit_reason();
                m_parent->run_later([rsn, hdr] {
                    CPPA_LOGC_TRACE("cppa::io::default_actor_proxy",
                                    "forward_msg$bouncer",
                                    "bounce message for reason " << rsn);
                    detail::sync_request_bouncer f{rsn};
                    f(hdr.sender.get(), hdr.id);
                });
                return; // no need to forward message
            }
            default: break;
        }
    }
    auto node = m_pinf;
    auto proto = m_parent;
    m_parent->run_later([hdr, msg, node, proto] {
        CPPA_LOGC_TRACE("cppa::io::default_actor_proxy",
                        "forward_msg$forwarder",
                        "");
        proto->enqueue(*node, hdr, msg);
    });
}

void default_actor_proxy::enqueue(const message_header& hdr, any_tuple msg) {
    CPPA_LOG_TRACE(CPPA_TARG(hdr, to_string) << ", " << CPPA_TARG(msg, to_string));
    auto& arr = detail::static_types_array<atom_value, uint32_t>::arr;
    if (   msg.size() == 2
        && msg.type_at(0) == arr[0]
        && msg.get_as<atom_value>(0) == atom("KILL_PROXY")
        && msg.type_at(1) == arr[1]) {
        CPPA_LOG_DEBUG("received KILL_PROXY message");
        intrusive_ptr<default_actor_proxy> _this{this};
        auto reason = msg.get_as<uint32_t>(1);
        m_parent->run_later([_this, reason] {
            CPPA_LOGC_TRACE("cppa::io::default_actor_proxy",
                            "enqueue$kill_proxy_helper",
                            "KILL_PROXY with exit reason " << reason);
            _this->cleanup(reason);
            detail::sync_request_bouncer f{reason};
            _this->m_pending_requests.close([&](const sync_request_info& e) {
                f(e.sender.get(), e.mid);
            });
        });
    }
    else forward_msg(hdr, move(msg));
}

void default_actor_proxy::link_to(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (link_to_impl(other)) {
        // causes remote actor to link to (proxy of) other
        // receiving peer will call: this->local_link_to(other)
        forward_msg({this, this}, make_any_tuple(atom("LINK"), other));
    }
}

void default_actor_proxy::unlink_from(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (unlink_from_impl(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg({this, this}, make_any_tuple(atom("UNLINK"), other));
    }
}

bool default_actor_proxy::establish_backlink(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (super::establish_backlink(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg({this, this}, make_any_tuple(atom("LINK"), other));
        return true;
    }
    return false;
}

bool default_actor_proxy::remove_backlink(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (super::remove_backlink(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg({this, this}, make_any_tuple(atom("UNLINK"), other));
        return true;
    }
    return false;
}

void default_actor_proxy::local_link_to(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    link_to_impl(other);
}

void default_actor_proxy::local_unlink_from(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    unlink_from_impl(other);
}

} } // namespace cppa::network
