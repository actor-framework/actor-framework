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


#ifndef CPPA_GROUP_HPP
#define CPPA_GROUP_HPP

#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_group.hpp"

#include "cppa/util/comparable.hpp"
#include "cppa/util/type_traits.hpp"

namespace cppa {

class channel;
class any_tuple;
class message_header;

struct invalid_group_t { constexpr invalid_group_t() { } };

namespace detail {
class raw_access;
} // namespace detail

/**
 * @brief Identifies an invalid {@link group}.
 * @relates group
 */
constexpr invalid_group_t invalid_group = invalid_group_t{};

class group : util::comparable<group>
            , util::comparable<group, invalid_group_t> {

    friend class detail::raw_access;

 public:

    group() = default;

    group(group&&) = default;

    group(const group&) = default;

    group(const invalid_group_t&);

    group& operator=(group&&) = default;

    group& operator=(const group&) = default;

    group& operator=(const invalid_group_t&);

    group(intrusive_ptr<abstract_group> ptr);

    inline explicit operator bool() const {
        return static_cast<bool>(m_ptr);
    }

    inline bool operator!() const {
        return !static_cast<bool>(m_ptr);
    }

    /**
     * @brief Returns a handle that grants access to
     *        actor operations such as enqueue.
     */
    inline abstract_group* operator->() const {
        return m_ptr.get();
    }

    inline abstract_group& operator*() const {
        return *m_ptr;
    }

    intptr_t compare(const group& other) const;

    inline intptr_t compare(const invalid_actor_t&) const {
        return m_ptr ? 1 : 0;
    }

    /**
     * @brief Get a pointer to the group associated with
     *        @p group_identifier from the module @p module_name.
     * @threadsafe
     */
    static group get(const std::string& module_name,
                     const std::string& group_identifier);

    /**
     * @brief Returns an anonymous group.
     *
     * Each calls to this member function returns a new instance
     * of an anonymous group. Anonymous groups can be used whenever
     * a set of actors wants to communicate using an exclusive channel.
     */
    static group anonymous();

    /**
     * @brief Add a new group module to the libcppa group management.
     * @threadsafe
     */
    static void add_module(abstract_group::unique_module_ptr);

    /**
     * @brief Returns the module associated with @p module_name.
     * @threadsafe
     */
    static abstract_group::module_ptr get_module(const std::string& module_name);


 private:

    abstract_group_ptr m_ptr;

};

} // namespace cppa

#endif // CPPA_GROUP_HPP
