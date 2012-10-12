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
 * Copyright (C) 2011, 2012                                                   *
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


#include <cstring>
#include <cstdint>

#include "cppa/on.hpp"
#include "cppa/actor.hpp"
#include "cppa/match.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"

#include "cppa/network/middleman.hpp"
#include "cppa/network/default_peer_impl.hpp"

using namespace std;

namespace cppa { namespace network {

default_peer_impl::default_peer_impl(middleman* parent,
                                     const input_stream_ptr& in,
                                     const output_stream_ptr& out,
                                     process_information_ptr peer_ptr)
: super(parent, in->read_handle(), out->write_handle()), m_in(in), m_out(out)
, m_state((peer_ptr) ? wait_for_msg_size : wait_for_process_info)
, m_peer(peer_ptr)
, m_meta_msg(uniform_typeid<addressed_message>())
, m_has_unwritten_data(false) {
    m_rd_buf.reset(m_state == wait_for_process_info
                   ? sizeof(uint32_t) + process_information::node_id_size
                   : sizeof(uint32_t));
}

default_peer_impl::~default_peer_impl() {
    if (m_peer) {
        // collect all children (proxies to actors of m_peer)
        vector<actor_proxy_ptr> children;
        children.reserve(20);
        detail::get_actor_proxy_cache().erase_all(m_peer->node_id(),
                                                  m_peer->process_id(),
                                                  [&](actor_proxy_ptr& pptr) {
            children.push_back(move(pptr));
        });
        // kill all proxies
        for (actor_proxy_ptr& pptr: children) {
            pptr->enqueue(nullptr,
                          make_any_tuple(atom("KILL_PROXY"),
                                         exit_reason::remote_link_unreachable));
        }
    }
}

continue_reading_result default_peer_impl::continue_reading() {
    for (;;) {
        try { m_rd_buf.append_from(m_in.get()); }
        catch (exception&) { return read_failure; }
        if (!m_rd_buf.full()) return read_continue_later; // try again later
        switch (m_state) {
            case wait_for_process_info: {
                //DEBUG("peer_connection::continue_reading: "
                //      "wait_for_process_info");
                uint32_t process_id;
                process_information::node_id_type node_id;
                memcpy(&process_id, m_rd_buf.data(), sizeof(uint32_t));
                memcpy(node_id.data(), m_rd_buf.data() + sizeof(uint32_t),
                       process_information::node_id_size);
                m_peer.reset(new process_information(process_id, node_id));
                if (*process_information::get() == *m_peer) {
                    std::cerr << "*** middleman warning: "
                                 "incoming connection from self"
                              << std::endl;
                    return read_failure;
                }
                register_peer(*m_peer);
                // initialization done
                m_state = wait_for_msg_size;
                m_rd_buf.reset(sizeof(uint32_t));
                //DEBUG("pinfo read: "
                //      << m_peer->process_id()
                //      << "@"
                //      << to_string(m_peer->node_id()));
                break;
            }
            case wait_for_msg_size: {
                //DEBUG("peer_connection::continue_reading: wait_for_msg_size");
                uint32_t msg_size;
                memcpy(&msg_size, m_rd_buf.data(), sizeof(uint32_t));
                m_rd_buf.reset(msg_size);
                m_state = read_message;
                break;
            }
            case read_message: {
                //DEBUG("peer_connection::continue_reading: read_message");
                addressed_message msg;
                binary_deserializer bd(m_rd_buf.data(), m_rd_buf.size());
                m_meta_msg->deserialize(&msg, &bd);
                auto& content = msg.content();
                //DEBUG("<-- " << to_string(msg));
                match(content) (
                    // monitor messages are sent automatically whenever
                    // actor_proxy_cache creates a new proxy
                    // note: aid is the *original* actor id
                    on(atom("MONITOR"), arg_match) >> [&](const process_information_ptr& pinfo, actor_id aid) {
                        if (!pinfo) {
                            //DEBUG("MONITOR received from invalid peer");
                            return;
                        }
                        auto ar = detail::singleton_manager::get_actor_registry();
                        auto reg_entry = ar->get_entry(aid);
                        auto pself = process_information::get();
                        auto send_kp = [=](uint32_t reason) {
                            parent()->enqueue(pinfo,
                                              nullptr,
                                              nullptr,
                                              make_any_tuple(
                                                  atom("KILL_PROXY"),
                                                  pself,
                                                  aid,
                                                  reason
                                              ));
                        };
                        if (reg_entry.first == nullptr) {
                            if (reg_entry.second == exit_reason::not_exited) {
                                // invalid entry
                                //DEBUG("MONITOR for an unknown actor received");
                            }
                            else {
                                // this actor already finished execution;
                                // reply with KILL_PROXY message
                                send_kp(reg_entry.second);
                            }
                        }
                        else {
                            reg_entry.first->attach_functor(send_kp);
                        }
                    },
                    on(atom("KILL_PROXY"), arg_match) >> [&](const process_information_ptr& peer, actor_id aid, std::uint32_t reason) {
                        auto& cache = detail::get_actor_proxy_cache();
                        auto proxy = cache.get(aid,
                                               peer->process_id(),
                                               peer->node_id());
                        if (proxy) {
                            proxy->enqueue(nullptr,
                                           make_any_tuple(
                                               atom("KILL_PROXY"), reason));
                        }
                        else {
                            //DEBUG("received KILL_PROXY message but didn't "
                            //      "found matching instance in cache");
                        }
                    },
                    on(atom("LINK"), arg_match) >> [&](const actor_ptr& ptr) {
                        if (msg.sender()->is_proxy() == false) {
                            //DEBUG("msg.sender() is not a proxy");
                            return;
                        }
                        auto whom = msg.sender().downcast<actor_proxy>();
                        if ((whom) && (ptr)) whom->local_link_to(ptr);
                    },
                    on(atom("UNLINK"), arg_match) >> [](const actor_ptr& ptr) {
                        if (ptr->is_proxy() == false) {
                            //DEBUG("msg.sender() is not a proxy");
                            return;
                        }
                        auto whom = ptr.downcast<actor_proxy>();
                        if ((whom) && (ptr)) whom->local_unlink_from(ptr);
                    },
                    others() >> [&] {
                        auto receiver = msg.receiver().get();
                        if (receiver) {
                            if (msg.id().valid()) {
                                auto ra = dynamic_cast<actor*>(receiver);
                                //DEBUG("sync message for actor " << ra->id());
                                if (ra) {
                                    ra->sync_enqueue(
                                        msg.sender().get(),
                                        msg.id(),
                                        move(msg.content()));
                                }
                                else{
                                    //DEBUG("ERROR: sync message to a non-actor");
                                }
                            }
                            else {
                                //DEBUG("async message (sender is "
                                //      << (msg.sender() ? "valid" : "NULL")
                                //      << ")");
                                receiver->enqueue(
                                    msg.sender().get(),
                                    move(msg.content()));
                            }
                        }
                        else {
                            //DEBUG("empty receiver");
                        }
                    }
                );
                m_rd_buf.reset(sizeof(uint32_t));
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

continue_writing_result default_peer_impl::continue_writing() {
    if (m_has_unwritten_data) {
        size_t written;
        try { written = m_out->write_some(m_wr_buf.data(), m_wr_buf.size()); }
        catch (exception&) { return write_failure; }
        if (written != m_wr_buf.size()) {
            m_wr_buf.erase_leading(written);
            return write_continue_later;
        }
        else {
            m_wr_buf.reset();
            m_has_unwritten_data = false;
            return write_done;
        }
    }
    return write_done;
}

bool default_peer_impl::enqueue(const addressed_message& msg) {
    binary_serializer bs(&m_wr_buf);
    uint32_t size = 0;
    auto before = m_wr_buf.size();
    m_wr_buf.write(sizeof(uint32_t), &size, util::grow_if_needed);
    try { bs << msg; }
    catch (exception& e) {
        cerr << "*** exception in default_peer_impl::enqueue; "
             << detail::demangle(typeid(e)) << ", what(): " << e.what()
             << endl;
        return false;
    }
    size = (m_wr_buf.size() - before) - sizeof(std::uint32_t);
    // update size in buffer
    memcpy(m_wr_buf.data() + before, &size, sizeof(std::uint32_t));
    if (!m_has_unwritten_data) {
        m_has_unwritten_data = true;
        begin_writing();
    }
    return true;
}


} } // namespace cppa::network
