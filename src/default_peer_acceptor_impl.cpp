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


#include <iostream>
#include <exception>

#include "cppa/process_information.hpp"

#include "cppa/network/default_peer_impl.hpp"
#include "cppa/network/default_peer_acceptor_impl.hpp"

#include "cppa/detail/demangle.hpp"

using namespace std;

namespace cppa { namespace network {

default_peer_acceptor_impl::default_peer_acceptor_impl(middleman* mm,
                                                       acceptor_uptr aur,
                                                       const actor_ptr& pa)
: super(mm, aur->file_handle(), pa), m_ptr(std::move(aur)) { }

continue_reading_result default_peer_acceptor_impl::continue_reading() {
    for (;;) {
        option<io_stream_ptr_pair> opt;
        try { opt = m_ptr->try_accept_connection(); }
        catch (...) { return read_failure; }
        if (opt) {
            auto& pair = *opt;
            auto& pself = process_information::get();
            uint32_t process_id = pself->process_id();
            try {
                actor_id aid = published_actor()->id();
                pair.second->write(&aid, sizeof(actor_id));
                pair.second->write(&process_id, sizeof(uint32_t));
                pair.second->write(pself->node_id().data(),
                                   pself->node_id().size());
                add_peer(new default_peer_impl(parent(), pair.first, pair.second));
            }
            catch (exception& e) {
                cerr << "*** exception while sending actor and process id; "
                     << detail::demangle(typeid(e))
                     << ", what(): " << e.what()
                     << endl;
            }
        }
        else return read_continue_later;
   }
}

} } // namespace cppa::network
