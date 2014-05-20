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


#ifndef CPPA_IO_TCP_PEER_ACCEPTOR_HPP
#define CPPA_IO_TCP_PEER_ACCEPTOR_HPP

#include "cppa/actor.hpp"

#include "cppa/io/tcp_acceptor.hpp"
#include "cppa/io/continuable.hpp"

namespace cppa {
namespace io {

class default_protocol;

class peer_acceptor : public continuable {

    typedef continuable super;

 public:

    typedef std::set<std::string> string_set;

    continue_reading_result continue_reading() override;

    peer_acceptor(middleman* parent,
                  acceptor_uptr ptr,
                  const actor_addr& published_actor,
                  string_set signatures);

    inline const actor_addr& published_actor() const { return m_aa; }

    void dispose() override;

    void io_failed(event_bitmask) override;

 private:

    middleman*    m_parent;
    acceptor_uptr m_ptr;
    actor_addr    m_aa;
    string_set    m_sigs;

};

} // namespace io
} // namespace cppa

#endif // CPPA_IO_TCP_PEER_ACCEPTOR_HPP
