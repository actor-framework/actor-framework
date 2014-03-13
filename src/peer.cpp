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


#include <cstring>
#include <cstdint>

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/match.hpp"
#include "cppa/logging.hpp"
#include "cppa/to_string.hpp"
#include "cppa/singletons.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/message_header.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/util/algorithm.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/raw_access.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"

#include "cppa/io/peer.hpp"
#include "cppa/io/middleman.hpp"

using namespace std;

namespace cppa { namespace io {

peer::peer(middleman* parent,
           const input_stream_ptr& in,
           const output_stream_ptr& out,
           node_id_ptr peer_ptr)
: super(parent, out, in->read_handle(), out->write_handle())
, m_in(in), m_state((peer_ptr) ? wait_for_msg_size : wait_for_process_info)
, m_node(peer_ptr) {
    m_rd_buf.final_size( m_state == wait_for_process_info
                       ? sizeof(uint32_t) + node_id::host_id_size
                       : sizeof(uint32_t));
    // state == wait_for_msg_size iff peer was created using remote_peer()
    // in this case, this peer must be erased if no proxy of it remains
    m_stop_on_last_proxy_exited = m_state == wait_for_msg_size;
    m_meta_hdr = uniform_typeid<message_header>();
    m_meta_msg = uniform_typeid<any_tuple>();
}

void peer::io_failed(event_bitmask mask) {
    CPPA_LOG_TRACE("node = " << (m_node ? to_string(*m_node) : "nullptr")
                   << " mask = " << mask);
    // make sure this code is executed only once by filtering for read failure
    if (mask == event::read && m_node) {
        // kill all proxies
        auto& children = parent()->get_namespace().proxies(*m_node);
        for (auto& kvp : children) {
            auto ptr = kvp.second.promote();
            if (ptr) {
                send_as(ptr, ptr, atom("KILL_PROXY"),
                        exit_reason::remote_link_unreachable);
            }
        }
        parent()->get_namespace().erase(*m_node);
    }
}

continue_reading_result peer::continue_reading() {
    CPPA_LOG_TRACE("");
    for (;;) {
        try { m_rd_buf.append_from(m_in.get()); }
        catch (exception&) {
            return read_failure;
        }
        if (!m_rd_buf.full()) return read_continue_later; // try again later
        switch (m_state) {
            case wait_for_process_info: {
                //DEBUG("peer_connection::continue_reading: "
                //      "wait_for_process_info");
                uint32_t process_id;
                node_id::host_id_type host_id;
                memcpy(&process_id, m_rd_buf.data(), sizeof(uint32_t));
                memcpy(host_id.data(), m_rd_buf.offset_data(sizeof(uint32_t)),
                       node_id::host_id_size);
                m_node.reset(new node_id(process_id, host_id));
                if (*parent()->node() == *m_node) {
                    std::cerr << "*** middleman warning: "
                                 "incoming connection from self"
                              << std::endl;
                    return read_failure;
                }
                CPPA_LOG_DEBUG("read process info: " << to_string(*m_node));
                if (!parent()->register_peer(*m_node, this)) {
                    CPPA_LOG_ERROR("multiple incoming connections "
                                   "from the same node");
                    return read_failure;
                }
                // initialization done
                m_state = wait_for_msg_size;
                m_rd_buf.clear();
                m_rd_buf.final_size(sizeof(uint32_t));
                break;
            }
            case wait_for_msg_size: {
                //DEBUG("peer_connection::continue_reading: wait_for_msg_size");
                uint32_t msg_size;
                memcpy(&msg_size, m_rd_buf.data(), sizeof(uint32_t));
                m_rd_buf.clear();
                m_rd_buf.final_size(msg_size);
                m_state = read_message;
                break;
            }
            case read_message: {
                //DEBUG("peer_connection::continue_reading: read_message");
                message_header hdr;
                any_tuple msg;
                binary_deserializer bd(m_rd_buf.data(), m_rd_buf.size(),
                                       &(parent()->get_namespace()), &m_incoming_types);
                try {
                    m_meta_hdr->deserialize(&hdr, &bd);
                    m_meta_msg->deserialize(&msg, &bd);
                }
                catch (exception& e) {
                    CPPA_LOG_ERROR("exception during read_message: "
                                   << detail::demangle(typeid(e))
                                   << ", what(): " << e.what());
                    return read_failure;
                }
                CPPA_LOG_DEBUG("deserialized: " << to_string(hdr) << " " << to_string(msg));
                match(msg) (
                    // monitor messages are sent automatically whenever
                    // actor_proxy_cache creates a new proxy
                    // note: aid is the *original* actor id
                    on(atom("MONITOR"), arg_match) >> [&](const node_id_ptr& node, actor_id aid) {
                        monitor(hdr.sender, node, aid);
                    },
                    on(atom("KILL_PROXY"), arg_match) >> [&](const node_id_ptr& node, actor_id aid, std::uint32_t reason) {
                        kill_proxy(hdr.sender, node, aid, reason);
                    },
                    on(atom("LINK"), arg_match) >> [&](const actor_addr& ptr) {
                        link(hdr.sender, ptr);
                    },
                    on(atom("UNLINK"), arg_match) >> [&](const actor_addr& ptr) {
                        unlink(hdr.sender, ptr);
                    },
                    on(atom("ADD_TYPE"), arg_match) >> [&](std::uint32_t id, const std::string& name) {
                        auto imap = get_uniform_type_info_map();
                        auto uti = imap->by_uniform_name(name);
                        m_incoming_types.emplace(id, uti);
                    },
                    others() >> [&] {
                        deliver(hdr, move(msg));
                    }
                );
                m_rd_buf.clear();
                m_rd_buf.final_size(sizeof(uint32_t));
                m_state = wait_for_msg_size;
                break;
            }
            default: {
                CPPA_CRITICAL("illegal state");
            }
        }
        // try to read more (next iteration)
    }
}

void peer::monitor(const actor_addr&,
                   const node_id_ptr& node,
                   actor_id aid) {
    CPPA_LOG_TRACE(CPPA_MARG(node, get) << ", " << CPPA_ARG(aid));
    if (!node) {
        CPPA_LOG_ERROR("received MONITOR from invalid peer");
        return;
    }
    auto entry = get_actor_registry()->get_entry(aid);
    auto pself = parent()->node();

    if (*node == *pself) {
        CPPA_LOG_ERROR("received 'MONITOR' from pself");
    }
    else if (entry.first == nullptr) {
        if (entry.second == exit_reason::not_exited) {
            CPPA_LOG_ERROR("received MONITOR for unknown "
                           "actor id: " << aid);
        }
        else {
            CPPA_LOG_DEBUG("received MONITOR for an actor "
                           "that already finished "
                           "execution; reply KILL_PROXY");
            // this actor already finished execution;
            // reply with KILL_PROXY message
            // get corresponding peer
            enqueue(make_any_tuple(atom("KILL_PROXY"), pself, aid, entry.second));
        }
    }
    else {
        CPPA_LOG_DEBUG("attach functor to " << entry.first.get());
        auto mm = parent();
        entry.first->attach_functor([=](uint32_t reason) {
            mm->run_later([=] {
                CPPA_LOGC_TRACE("cppa::io::peer",
                                "monitor$kill_proxy_helper",
                                "reason = " << reason);
                auto p = mm->get_peer(*node);
                if (p) p->enqueue(make_any_tuple(atom("KILL_PROXY"), pself, aid, reason));
            });
        });
    }
}

void peer::kill_proxy(const actor_addr& sender,
                      const node_id_ptr& node,
                      actor_id aid,
                      std::uint32_t reason) {
    CPPA_LOG_TRACE(CPPA_TARG(sender, to_string)
                   << ", node = " << (node ? to_string(*node) : "-invalid-")
                   << ", " << CPPA_ARG(aid)
                   << ", " << CPPA_ARG(reason));
    if (!node) {
        CPPA_LOG_ERROR("node = nullptr");
        return;
    }
    if (sender != nullptr) {
        CPPA_LOG_ERROR("sender != nullptr");
        return;
    }
    auto proxy = parent()->get_namespace().get(*node, aid);
    if (proxy) {
        CPPA_LOG_DEBUG("received KILL_PROXY for " << aid
                       << ":" << to_string(*node));
        send_as(proxy, proxy, atom("KILL_PROXY"), reason);
    }
    else {
        CPPA_LOG_INFO("received KILL_PROXY for " << aid
                      << ":" << to_string(*node)
                      << "but didn't found a matching instance "
                      << "in proxy cache");
    }
}

void peer::deliver(msg_hdr_cref hdr, any_tuple msg) {
    CPPA_LOG_TRACE("");
    if (hdr.sender && hdr.sender.is_remote()) {
        // is_remote() is guaranteed to return true if and only if
        // the actor is derived from actor_proxy, hence we do not
        // need to use a dynamic_cast here
        auto ptr = static_cast<actor_proxy*>(detail::raw_access::get(hdr.sender));
        ptr->deliver(hdr, std::move(msg));
    }
    else hdr.deliver(std::move(msg));
}

void peer::link(const actor_addr& lhs, const actor_addr& rhs) {
    // this message is sent from default_actor_proxy in link_to and
    // establish_backling to cause the original actor (sender) to establish
    // a link to ptr as well
    CPPA_LOG_TRACE(CPPA_TARG(lhs, to_string) << ", "
                   << CPPA_TARG(rhs, to_string));
    CPPA_LOG_ERROR_IF(!lhs, "received 'LINK' from invalid sender");
    CPPA_LOG_ERROR_IF(!rhs, "received 'LINK' with invalid receiver");
    if (!lhs || !rhs) return;
    auto locally_link_proxy = [](const actor_addr& proxy, const actor_addr& addr) {
        // again, no need to to use a dynamic_cast here
        auto ptr = static_cast<actor_proxy*>(detail::raw_access::get(proxy));
        ptr->local_link_to(addr);
    };
    switch ((lhs.is_remote() ? 0x10 : 0x00) | (rhs.is_remote() ? 0x01 : 0x00)) {
        case 0x00: // both local
        case 0x11: // both remote
            detail::raw_access::get(lhs)->link_to(rhs);
            break;
        case 0x10: // sender is remote
            locally_link_proxy(lhs, rhs);
            break;
        case 0x01: // receiver is remote
            locally_link_proxy(rhs, lhs);
            break;
        default: CPPA_LOG_ERROR("logic error");
    }
}

void peer::unlink(const actor_addr& lhs, const actor_addr& rhs) {
    CPPA_LOG_TRACE(CPPA_TARG(lhs, to_string) << ", "
                   << CPPA_TARG(rhs, to_string));
    CPPA_LOG_ERROR_IF(!lhs, "received 'UNLINK' from invalid sender");
    CPPA_LOG_ERROR_IF(!rhs, "received 'UNLINK' with invalid target");
    if (!lhs || !rhs) return;
    auto locally_unlink_proxy = [](const actor_addr& proxy, const actor_addr& addr) {
        // again, no need to to use a dynamic_cast here
        auto ptr = static_cast<actor_proxy*>(detail::raw_access::get(proxy));
        ptr->local_unlink_from(addr);
    };
    switch ((lhs.is_remote() ? 0x10 : 0x00) | (rhs.is_remote() ? 0x01 : 0x00)) {
        case 0x00: // both local
        case 0x11: // both remote
            detail::raw_access::get(lhs)->unlink_from(rhs);
            break;
        case 0x10: // sender is remote
            locally_unlink_proxy(lhs, rhs);
            break;
        case 0x01: // receiver is remote
            locally_unlink_proxy(rhs, lhs);
            break;
        default: CPPA_LOG_ERROR("logic error");
    }
}

continue_writing_result peer::continue_writing() {
    CPPA_LOG_TRACE("");
    auto result = super::continue_writing();
    while (result == write_done && !queue().empty()) {
        auto tmp = queue().pop();
        enqueue(tmp.first, tmp.second);
        result = super::continue_writing();
    }
    if (result == write_done && stop_on_last_proxy_exited() && !has_unwritten_data()) {
        if (parent()->get_namespace().count_proxies(*m_node) == 0) {
            parent()->last_proxy_exited(this);
        }
    }
    return result;
}

void peer::add_type_if_needed(const std::string& tname) {
    if (m_outgoing_types.id_of(tname) == 0) {
        auto id = m_outgoing_types.max_id() + 1;
        auto imap = get_uniform_type_info_map();
        auto uti = imap->by_uniform_name(tname);
        m_outgoing_types.emplace(id, uti);
        enqueue_impl({invalid_actor_addr, nullptr}, make_any_tuple(atom("ADD_TYPE"), id, tname));
    }
}

void peer::enqueue_impl(msg_hdr_cref hdr, const any_tuple& msg) {
    CPPA_LOG_TRACE("");
    auto tname = msg.tuple_type_names();
    add_type_if_needed((tname) ? *tname : detail::get_tuple_type_names(*msg.vals()));
    uint32_t size = 0;
    auto& wbuf = write_buffer();
    auto before = wbuf.size();
    binary_serializer bs(&wbuf, &(parent()->get_namespace()), &m_outgoing_types);
    wbuf.write(sizeof(uint32_t), &size);
    try { bs << hdr << msg; }
    catch (exception& e) {
        CPPA_LOG_ERROR(to_verbose_string(e));
        cerr << "*** exception in peer::enqueue; "
             << to_verbose_string(e)
             << endl;
        return;
    }
    CPPA_LOG_DEBUG("serialized: " << to_string(hdr) << " " << to_string(msg));
    size = (wbuf.size() - before) - sizeof(std::uint32_t);
    // update size in buffer
    memcpy(wbuf.offset_data(before), &size, sizeof(std::uint32_t));
}

void peer::enqueue(msg_hdr_cref hdr, const any_tuple& msg) {
    enqueue_impl(hdr, msg);
    register_for_writing();
}

void peer::dispose() {
    CPPA_LOG_TRACE(CPPA_ARG(this));
    parent()->get_namespace().erase(*m_node);
    parent()->del_peer(this);
    delete this;
}

} } // namespace cppa::network
