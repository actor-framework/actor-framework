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


#include <iostream>
#include <exception>

#include "cppa/logging.hpp"
#include "cppa/node_id.hpp"
#include "cppa/to_string.hpp"

#include "cppa/io/peer.hpp"
#include "cppa/io/peer_acceptor.hpp"

#include "cppa/detail/demangle.hpp"

using namespace std;

namespace cppa {
namespace io {

peer_acceptor::peer_acceptor(middleman* parent,
                             acceptor_uptr aur,
                             const actor_addr& addr,
                             string_set sigs)
        : super(aur->file_handle()), m_parent(parent), m_ptr(std::move(aur))
        , m_aa(addr), m_sigs(std::move(sigs)) { }

continue_reading_result peer_acceptor::continue_reading() {
    CPPA_LOG_TRACE("");
    for (;;) {
        optional<stream_ptr_pair> opt{none};
        try { opt = m_ptr->try_accept_connection(); }
        catch (exception& e) {
            CPPA_LOG_ERROR(to_verbose_string(e));
            static_cast<void>(e); // keep compiler happy
            return continue_reading_result::failure;
        }
        if (opt) {
            auto& pair = *opt;
            auto& pself = m_parent->node();
            uint32_t process_id = pself->process_id();
            try {
                auto& out = pair.second;
                actor_id aid = published_actor().id();
                // serialize: actor id, process id, node id, interface
                out->write(&aid, sizeof(actor_id));
                out->write(&process_id, sizeof(uint32_t));
                out->write(pself->host_id().data(),
                                   pself->host_id().size());
                auto u32_size = static_cast<std::uint32_t>(m_sigs.size());
                out->write(&u32_size, sizeof(uint32_t));
                for (auto& sig : m_sigs) {
                    u32_size = static_cast<std::uint32_t>(sig.size());
                    out->write(&u32_size, sizeof(uint32_t));
                    out->write(sig.c_str(), sig.size());
                }
                m_parent->new_peer(pair.first, pair.second);
            }
            catch (exception& e) {
                CPPA_LOG_ERROR(to_verbose_string(e));
                cerr << "*** exception while sending actor and process id; "
                     << to_verbose_string(e)
                     << endl;
            }
        }
        else return continue_reading_result::continue_later;
   }
}

void peer_acceptor::io_failed(event_bitmask) {
    CPPA_LOG_INFO("removed peer_acceptor "
                  << this << " due to an IO failure");
}

void peer_acceptor::dispose() {
    m_parent->del_acceptor(this);
    delete this;
}

} // namespace io
} // namespace cppa

