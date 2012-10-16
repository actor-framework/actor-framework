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


#ifndef MIDDLEMAN_HPP
#define MIDDLEMAN_HPP

#include <map>
#include <vector>
#include <memory>
#include <functional>

#include "cppa/network/protocol.hpp"
#include "cppa/network/acceptor.hpp"
#include "cppa/network/continuable_reader.hpp"
#include "cppa/network/continuable_writer.hpp"

namespace cppa { namespace detail { class singleton_manager; } }

namespace cppa { namespace network {

class middleman_impl;
class middleman_overseer;
class middleman_event_handler;

void middleman_loop(middleman_impl*);

class middleman {

    // the most popular class in libcppa

    friend class peer;
    friend class protocol;
    friend class peer_acceptor;
    friend class singleton_manager;
    friend class middleman_overseer;
    friend class middleman_event_handler;
    friend class detail::singleton_manager;

    friend void middleman_loop(middleman_impl*);

 public:

    virtual ~middleman();

    virtual void add_protocol(const protocol_ptr& impl) = 0;

    virtual protocol_ptr protocol(atom_value id) = 0;

 protected:

    middleman();

    virtual void stop() = 0;
    virtual void start() = 0;

    // to be called from protocol

    // runs @p fun in the middleman's event loop
    virtual void run_later(std::function<void()> fun) = 0;


    // to be called from singleton_manager

    static middleman* create_singleton();


    // to be called from peer

    /**
     * @pre ptr->as_writer() != nullptr
     */
    void continue_writer(const continuable_reader_ptr& ptr);

    /**
     * @pre ptr->as_writer() != nullptr
     */
    void stop_writer(const continuable_reader_ptr& ptr);

    // to be called form peer_acceptor or protocol

    void continue_reader(const continuable_reader_ptr& what);

    void stop_reader(const continuable_reader_ptr& what);


    // to be called from m_handler or middleman_overseer

    inline void quit() { m_done = true; }
    inline bool done() const { return m_done; }


    // member variables

    bool m_done;
    std::vector<continuable_reader_ptr> m_readers;
    std::unique_ptr<middleman_event_handler> m_handler;

};

} } // namespace cppa::detail

#endif // MIDDLEMAN_HPP
