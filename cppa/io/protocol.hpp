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


#ifndef CPPA_PROTOCOL_HPP
#define CPPA_PROTOCOL_HPP

#include <memory>
#include <functional>
#include <initializer_list>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/primitive_variant.hpp"

#include "cppa/io/acceptor.hpp"
#include "cppa/io/middleman.hpp"

namespace cppa { class actor_addressing; }

namespace cppa { namespace io {

class continuable;
class abstract_middleman;

/**
 * @brief Implements a communication protocol.
 */
class protocol {

 public:

    typedef std::initializer_list<primitive_variant> variant_args;

    protocol(middleman* parent);

    virtual atom_value identifier() const = 0;

    virtual void publish(const actor_ptr& whom, variant_args args) = 0;

    virtual void publish(const actor_ptr& whom,
                         std::unique_ptr<acceptor> acceptor,
                         variant_args args                  ) = 0;

    virtual void unpublish(const actor_ptr& whom) = 0;

    virtual actor_ptr remote_actor(variant_args args) = 0;

    virtual actor_ptr remote_actor(stream_ptr_pair ioptrs,
                                   variant_args args         ) = 0;

    virtual actor_addressing* addressing() = 0;

    /**
     * @brief Convenience member function to be used by children of
     *        this protocol.
     */
    template<typename T>
    inline void run_later(T&& what);

    /**
     * @brief Convenience member function to be used by children of
     *        this protocol.
     */
    inline void stop_writer(continuable* ptr);

    /**
     * @brief Convenience member function to be used by children of
     *        this protocol.
     */
    inline void continue_writer(continuable* ptr);

    /**
     * @brief Convenience member function to be used by children of
     *        this protocol.
     */
    inline void stop_reader(continuable* ptr);

    /**
     * @brief Convenience member function to be used by children of
     *        this protocol.
     */
    inline void continue_reader(continuable* ptr);

    /**
     * @brief Returns the parent of this protocol instance.
     */
    inline middleman* parent();

 private:

    middleman* m_parent;

};

typedef intrusive_ptr<protocol> protocol_ptr;

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline middleman* protocol::parent() {
    return m_parent;
}

template<typename T>
inline void protocol::run_later(T&& what) {
    m_parent->run_later(std::forward<T>(what));
}

inline void protocol::stop_writer(continuable* ptr) {
    m_parent->stop_writer(ptr);
}

inline void protocol::continue_writer(continuable* ptr) {
    m_parent->continue_writer(ptr);
}

inline void protocol::stop_reader(continuable* ptr) {
    m_parent->stop_reader(ptr);
}

inline void protocol::continue_reader(continuable* ptr) {
    m_parent->continue_reader(ptr);
}

} } // namespace cppa::network

#endif // CPPA_PROTOCOL_HPP
