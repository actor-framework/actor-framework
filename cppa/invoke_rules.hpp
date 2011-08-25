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
 * Copyright (C) 2011, Dominik Charousset <dominik.charousset@haw-hamburg.de> *
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

#ifndef INVOKE_RULES_HPP
#define INVOKE_RULES_HPP

#include <list>
#include <memory>

namespace cppa { namespace detail { class invokable; class intermediate; } }

namespace cppa {

class any_tuple;

struct invoke_rules
{

    typedef std::list< std::unique_ptr<detail::invokable> > list_type;

    list_type m_list;

    invoke_rules(const invoke_rules&) = delete;
    invoke_rules& operator=(const invoke_rules&) = delete;

    invoke_rules(list_type&& ll);

 public:

    invoke_rules() = default;

    invoke_rules(invoke_rules&& other);

    invoke_rules(detail::invokable* arg);

    invoke_rules(std::unique_ptr<detail::invokable>&& arg);

    bool operator()(const any_tuple& t) const;

    detail::intermediate* get_intermediate(const any_tuple& t) const;

    invoke_rules& splice(invoke_rules&& other);

    invoke_rules operator,(invoke_rules&& other);

};

} // namespace cppa

#endif // INVOKE_RULES_HPP
