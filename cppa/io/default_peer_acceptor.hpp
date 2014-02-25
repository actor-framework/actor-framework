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


#ifndef IPV4_PEER_ACCEPTOR_HPP
#define IPV4_PEER_ACCEPTOR_HPP

#include "cppa/actor.hpp"

#include "cppa/io/ipv4_acceptor.hpp"
#include "cppa/io/continuable.hpp"

namespace cppa { namespace io {

class default_protocol;

class default_peer_acceptor : public continuable {

    typedef continuable super;

 public:

    continue_reading_result continue_reading() override;

    default_peer_acceptor(default_protocol* parent,
                          acceptor_uptr ptr,
                          const actor_ptr& published_actor);

    inline const actor_ptr& published_actor() const { return m_pa; }

    void dispose() override;

    void io_failed(event_bitmask) override;

 private:

    default_protocol* m_parent;
    acceptor_uptr m_ptr;
    actor_ptr m_pa;

};

} } // namespace cppa::detail

#endif // IPV4_PEER_ACCEPTOR_HPP
