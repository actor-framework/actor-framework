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


#ifndef CPPA_PROTOCOL_HPP
#define CPPA_PROTOCOL_HPP

#include <memory>
#include <functional>
#include <initializer_list>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/primitive_variant.hpp"

#include "cppa/network/acceptor.hpp"

namespace cppa { class actor_addressing; }

namespace cppa { namespace network {

class abstract_middleman;
class continuable_reader;
class continuable_io;

/**
 * @brief Implements a communication protocol.
 */
class protocol : public ref_counted {

    typedef ref_counted super;

 public:

    typedef std::initializer_list<primitive_variant> variant_args;

    protocol(abstract_middleman* parent);

    virtual atom_value identifier() const = 0;

    virtual void publish(const actor_ptr& whom, variant_args args) = 0;

    virtual void publish(const actor_ptr& whom,
                         std::unique_ptr<acceptor> acceptor,
                         variant_args args                  ) = 0;

    virtual void unpublish(const actor_ptr& whom) = 0;

    virtual actor_ptr remote_actor(variant_args args) = 0;

    virtual actor_ptr remote_actor(io_stream_ptr_pair ioptrs,
                                   variant_args args         ) = 0;

    virtual actor_addressing* addressing() = 0;

    void run_later(std::function<void()> fun);

 protected:

    // note: not thread-safe; call only in run_later functor!
    void continue_reader(continuable_reader* what);

    // note: not thread-safe; call only in run_later functor!
    void continue_writer(continuable_reader* what);

    // note: not thread-safe; call only in run_later functor!
    void stop_reader(continuable_reader* what);

    // note: not thread-safe; call only in run_later functor!
    void stop_writer(continuable_reader* what);

    inline abstract_middleman* parent() { return m_parent; }

    inline const abstract_middleman* parent() const { return m_parent; }

 private:

    abstract_middleman* m_parent;

};

typedef intrusive_ptr<protocol> protocol_ptr;

} } // namespace cppa::network

#endif // CPPA_PROTOCOL_HPP
