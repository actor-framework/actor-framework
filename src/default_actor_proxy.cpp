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


#include "cppa/to_string.hpp"

#include "cppa/logging.hpp"
#include "cppa/network/middleman.hpp"
#include "cppa/network/default_actor_proxy.hpp"

#include "cppa/detail/memory.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/sync_request_bouncer.hpp"

using namespace std;

namespace cppa { namespace network {

inline sync_request_info* new_req_info(actor_ptr sptr, message_id_t id) {
    return detail::memory::create<sync_request_info>(std::move(sptr), id);
}

sync_request_info::sync_request_info(actor_ptr sptr, message_id_t id)
: next(nullptr), sender(std::move(sptr)), mid(id) { }

default_actor_proxy::default_actor_proxy(actor_id mid,
                                         const process_information_ptr& pinfo,
                                         const default_protocol_ptr& parent)
: super(mid), m_proto(parent), m_pinf(pinfo) { }

default_actor_proxy::~default_actor_proxy() {
    auto aid = id();
    auto node = m_pinf;
    auto proto = m_proto;
    proto->run_later([aid, node, proto] {
        CPPA_LOGC_TRACE("cppa::network::default_actor_proxy",
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

void default_actor_proxy::deliver(const network::message_header& hdr, any_tuple msg) {
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

void default_actor_proxy::forward_msg(const actor_ptr& sender,
                                      any_tuple msg,
                                      message_id_t mid) {
    CPPA_LOG_TRACE("");
    if (sender && mid.is_request()) {
        switch (m_pending_requests.enqueue(new_req_info(sender, mid))) {
            case intrusive::queue_closed: {
                if (sender) {
                    auto rsn = exit_reason();
                    m_proto->run_later([rsn,sender,mid] {
                        CPPA_LOGC_TRACE("cppa::network::default_actor_proxy",
                                        "forward_msg$bouncer",
                                        "bounce message for reason " << rsn);
                        detail::sync_request_bouncer f{rsn};
                        f(sender.get(), mid);
                    });
                }
                return; // no need to forward message
            }
            default: break;
        }
    }
    message_header hdr{sender, this, mid};
    auto node = m_pinf;
    auto proto = m_proto;
    m_proto->run_later([hdr, msg, node, proto] {
        CPPA_LOGC_TRACE("cppa::network::default_actor_proxy",
                        "forward_msg$forwarder",
                        "");
        proto->enqueue(*node, hdr, msg);
    });
}

void default_actor_proxy::enqueue(const actor_ptr& sender, any_tuple msg) {
    CPPA_LOG_TRACE(CPPA_TARG(sender, to_string) << ", "
                   << CPPA_TARG(msg, to_string));
    auto& arr = detail::static_types_array<atom_value, uint32_t>::arr;
    if (   msg.size() == 2
        && msg.type_at(0) == arr[0]
        && msg.get_as<atom_value>(0) == atom("KILL_PROXY")
        && msg.type_at(1) == arr[1]) {
        CPPA_LOG_DEBUG("received KILL_PROXY message");
        intrusive_ptr<default_actor_proxy> _this{this};
        auto reason = msg.get_as<uint32_t>(1);
        m_proto->run_later([_this, reason] {
            CPPA_LOGC_TRACE("cppa::network::default_actor_proxy",
                            "enqueue$kill_proxy_helper",
                            "KILL_PROXY with exit reason " << reason);
            _this->cleanup(reason);
            detail::sync_request_bouncer f{reason};
            _this->m_pending_requests.close([&](const sync_request_info& e) {
                f(e.sender.get(), e.mid);
            });
        });
        return;
    }
    forward_msg(sender, move(msg));
}

void default_actor_proxy::sync_enqueue(const actor_ptr& sender,
                                       message_id_t mid,
                                       any_tuple msg) {
    CPPA_LOG_TRACE(CPPA_TARG(sender, to_string) << ", "
                   << CPPA_MARG(mid, integer_value) << ", "
                   << CPPA_TARG(msg, to_string));
    forward_msg(sender, move(msg), mid);
}

void default_actor_proxy::link_to(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (link_to_impl(other)) {
        // causes remote actor to link to (proxy of) other
        // receiving peer will call: this->local_link_to(other)
        forward_msg(this, make_any_tuple(atom("LINK"), other));
    }
}

void default_actor_proxy::unlink_from(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (unlink_from_impl(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(this, make_any_tuple(atom("UNLINK"), other));
    }
}

bool default_actor_proxy::establish_backlink(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (super::establish_backlink(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(this, make_any_tuple(atom("LINK"), other));
        return true;
    }
    return false;
}

bool default_actor_proxy::remove_backlink(const intrusive_ptr<actor>& other) {
    CPPA_LOG_TRACE(CPPA_MARG(other, get));
    if (super::remove_backlink(other)) {
        // causes remote actor to unlink from (proxy of) other
        forward_msg(this, make_any_tuple(atom("UNLINK"), other));
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
