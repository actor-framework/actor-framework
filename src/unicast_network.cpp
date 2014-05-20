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
 * Copyright (C) 2011-2014                                                    *
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


#include "cppa/config.hpp"

#include <ios> // ios_base::failure
#include <list>
#include <memory>
#include <cstring>    // memset
#include <iostream>
#include <stdexcept>
#include <condition_variable>

#ifndef CPPA_WINDOWS
#include <netinet/tcp.h>
#endif

#include "cppa/cppa.hpp"
#include "cppa/atom.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exception.hpp"
#include "cppa/singletons.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/binary_serializer.hpp"
#include "cppa/binary_deserializer.hpp"

#include "cppa/detail/raw_access.hpp"

#include "cppa/intrusive/single_reader_queue.hpp"

#include "cppa/io/acceptor.hpp"
#include "cppa/io/middleman.hpp"
#include "cppa/io/peer_acceptor.hpp"
#include "cppa/io/tcp_acceptor.hpp"
#include "cppa/io/tcp_io_stream.hpp"
#include "cppa/io/remote_actor_proxy.hpp"

namespace {

constexpr std::uint32_t max_iface_size = 100;

constexpr std::uint32_t max_iface_clause_size = 500;

typedef std::set<std::string> string_set;

} // namespace <anonymous>

namespace cppa {

using namespace io;

void publish(actor whom, std::uint16_t port, const char* addr) {
    if (!whom) return;
    publish(std::move(whom), io::tcp_acceptor::create(port, addr));
}

void publish(actor whom, std::unique_ptr<io::acceptor> acceptor) {
    detail::publish_impl(detail::raw_access::get(whom), std::move(acceptor));
}

actor remote_actor(io::stream_ptr_pair conn) {
    auto res = detail::remote_actor_impl(conn, string_set{});
    return detail::raw_access::unsafe_cast(res);
}

actor remote_actor(const char* host, std::uint16_t port) {
    auto ptr = tcp_io_stream::connect_to(host, port);
    return remote_actor(stream_ptr_pair(ptr, ptr));
}

namespace detail {

void publish_impl(abstract_actor_ptr ptr, std::unique_ptr<acceptor> aptr) {
    // begin the scenes, we serialze/deserialize as actor
    actor whom{raw_access::unsafe_cast(ptr.get())};
    CPPA_LOGF_TRACE(CPPA_TARG(whom, to_string) << ", " << CPPA_MARG(aptr, get));
    if (!whom) return;
    get_actor_registry()->put(whom->id(), detail::raw_access::get(whom));
    auto mm = get_middleman();
    auto addr = whom.address();
    auto sigs = whom->interface();
    mm->register_acceptor(addr, new peer_acceptor(mm, move(aptr),
                                                  addr, move(sigs)));
}

abstract_actor_ptr remote_actor_impl(stream_ptr_pair io, string_set expected) {
    CPPA_LOGF_TRACE("io{" << io.first.get() << ", " << io.second.get() << "}");
    auto mm = get_middleman();
    auto pinf = mm->node();
    std::uint32_t process_id = pinf->process_id();
    // throws on error
    io.second->write(&process_id, sizeof(std::uint32_t));
    io.second->write(pinf->host_id().data(), pinf->host_id().size());
    // deserialize: actor id, process id, node id, interface
    actor_id remote_aid;
    std::uint32_t peer_pid;
    node_id::host_id_type peer_node_id;
    std::uint32_t iface_size;
    std::set<std::string> iface;
    auto& in = io.first;
    // -> actor id
    in->read(&remote_aid, sizeof(actor_id));
    // -> process id
    in->read(&peer_pid, sizeof(std::uint32_t));
    // -> node id
    in->read(peer_node_id.data(), peer_node_id.size());
    // -> interface
    in->read(&iface_size, sizeof(std::uint32_t));
    if (iface_size > max_iface_size) {
        throw std::invalid_argument("Remote actor claims to have more than"
                                    +std::to_string(max_iface_size)+
                                    " message types? Someone is trying"
                                    " something nasty!");
    }
    std::vector<char> strbuf;
    for (std::uint32_t i = 0; i < iface_size; ++i) {
        std::uint32_t str_size;
        in->read(&str_size, sizeof(std::uint32_t));
        if (str_size > max_iface_clause_size) {
            throw std::invalid_argument("Remote actor claims to have a"
                                        " reply_to<...>::with<...> clause with"
                                        " more than"
                                        +std::to_string(max_iface_clause_size)+
                                        " characters? Someone is"
                                        " trying something nasty!");
        }
        strbuf.reserve(str_size + 1);
        strbuf.resize(str_size);
        in->read(strbuf.data(), str_size);
        strbuf.push_back('\0');
        iface.insert(std::string{strbuf.data()});
    }
    // deserialization done, check interface
    if (iface != expected) {
        auto tostr = [](const std::set<std::string>& what) -> std::string {
            if (what.empty()) return "actor";
            std::string tmp;
            tmp = "typed_actor<";
            auto i = what.begin();
            auto e = what.end();
            tmp += *i++;
            while (i != e) tmp += *i++;
            tmp += ">";
            return tmp;
        };
        auto iface_str = tostr(iface);
        auto expected_str = tostr(expected);
        if (expected.empty()) {
            throw std::invalid_argument("expected remote actor to be a "
                                        "dynamically typed actor but found "
                                        "a strongly typed actor of type "
                                        + iface_str);
        }
        if (iface.empty()) {
            throw std::invalid_argument("expected remote actor to be a "
                                        "strongly typed actor of type "
                                        + expected_str +
                                        " but found a dynamically typed actor");
        }
        throw std::invalid_argument("expected remote actor to be a "
                                    "strongly typed actor of type "
                                    + expected_str +
                                    " but found a strongly typed actor of type "
                                    + iface_str);
    }
    auto pinfptr = make_counted<node_id>(peer_pid, peer_node_id);
    if (*pinf == *pinfptr) {
        // this is a local actor, not a remote actor
        CPPA_LOGF_WARNING("remote_actor() called to access a local actor");
        auto ptr = get_actor_registry()->get(remote_aid);
        return ptr;
    }
    struct remote_actor_result { remote_actor_result* next; actor value; };
    std::mutex qmtx;
    std::condition_variable qcv;
    intrusive::single_reader_queue<remote_actor_result> q;
    mm->run_later([mm, io, pinfptr, remote_aid, &q, &qmtx, &qcv] {
        CPPA_LOGC_TRACE("cppa",
                        "remote_actor$create_connection", "");
        auto pp = mm->get_peer(*pinfptr);
        CPPA_LOGF_INFO_IF(pp, "connection already exists (re-use old one)");
        if (!pp) mm->new_peer(io.first, io.second, pinfptr);
        auto res = mm->get_namespace().get_or_put(pinfptr, remote_aid);
        q.synchronized_enqueue(qmtx, qcv, new remote_actor_result{0, res});
    });
    std::unique_ptr<remote_actor_result> result(q.synchronized_pop(qmtx, qcv));
    CPPA_LOGF_DEBUG(CPPA_MARG(result, get));
    return raw_access::get(result->value);
}

} // namespace util
} // namespace cppa

