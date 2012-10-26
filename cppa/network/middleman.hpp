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

/**
 * @brief Multiplexes asynchronous IO.
 */
class middleman {

    friend class detail::singleton_manager;

 public:

    virtual ~middleman();

    /**
     * @brief Add a new communication protocol to the middleman.
     */
    virtual void add_protocol(const protocol_ptr& impl) = 0;

    /**
     * @brief Returns the protocol associated with @p id.
     */
    virtual protocol_ptr protocol(atom_value id) = 0;

    /**
     * @brief Runs @p fun in the middleman's event loop.
     */
    virtual void run_later(std::function<void()> fun) = 0;

 protected:

    virtual void stop() = 0;
    virtual void start() = 0;

 private:

    static middleman* create_singleton();

};

class middleman_event_handler;

class abstract_middleman : public middleman {

 public:

    inline abstract_middleman() : m_done(false) { }

    void stop_writer(const continuable_reader_ptr& ptr);
    void continue_writer(const continuable_reader_ptr& ptr);

    void stop_reader(const continuable_reader_ptr& what);
    void continue_reader(const continuable_reader_ptr& what);

 protected:

    inline void quit() { m_done = true; }
    inline bool done() const { return m_done; }

    bool m_done;
    std::vector<continuable_reader_ptr> m_readers;

    middleman_event_handler& handler();

};

} } // namespace cppa::detail

#endif // MIDDLEMAN_HPP
