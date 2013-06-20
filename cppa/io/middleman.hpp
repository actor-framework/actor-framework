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


#ifndef MIDDLEMAN_HPP
#define MIDDLEMAN_HPP

#include <map>
#include <vector>
#include <memory>
#include <functional>

#include "cppa/io/continuable.hpp"

namespace cppa { namespace detail { class singleton_manager; } }

namespace cppa { namespace io {

class protocol;
class middleman_impl;

/**
 * @brief Multiplexes asynchronous IO.
 */
class middleman {

    friend class detail::singleton_manager;

 public:

    virtual ~middleman();

    /**
     * @brief Returns the networking protocol in use.
     */
    protocol* get_protocol();

    /**
     * @brief Runs @p fun in the middleman's event loop.
     */
    void run_later(std::function<void()> fun);

    /**
     * @brief Removes @p ptr from the list of active writers.
     * @warning This member function is not thread-safe.
     */
    void stop_writer(const continuable_ptr& ptr);

    /**
     * @brief Adds @p ptr to the list of active writers.
     * @warning This member function is not thread-safe.
     */
    void continue_writer(const continuable_ptr& ptr);

    /**
     * @brief Removes @p ptr from the list of active readers.
     * @warning This member function is not thread-safe.
     */
    void stop_reader(const continuable_ptr& ptr);

    /**
     * @brief Adds @p ptr to the list of active readers.
     * @warning This member function is not thread-safe.
     */
    void continue_reader(const continuable_ptr& ptr);

 protected:

    // destroys singleton
    void destroy();

    // initializes singletons
    void initialize();

 private:

    // sets m_impl and binds implementation to given protocol
    void set_pimpl(std::unique_ptr<protocol>&&);

    // creates a middleman using io::default_protocol
    static middleman* create_singleton();

    // destroys uninitialized instances
    inline void dispose() { delete this; }

    // pointer to implementation
    std::unique_ptr<middleman_impl> m_impl;

};

} } // namespace cppa::detail

#endif // MIDDLEMAN_HPP
