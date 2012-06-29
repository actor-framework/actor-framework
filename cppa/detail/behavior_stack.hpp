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


#ifndef BEHAVIOR_STACK_HPP
#define BEHAVIOR_STACK_HPP

#include <vector>
#include <memory>

#include "cppa/behavior.hpp"
#include "cppa/detail/disablable_delete.hpp"

namespace cppa { namespace detail {

class behavior_stack
{

    behavior_stack(const behavior_stack&) = delete;
    behavior_stack& operator=(const behavior_stack&) = delete;

 public:

    behavior_stack() = default;

    typedef detail::disablable_delete<behavior> deleter;
    typedef std::unique_ptr<behavior, deleter> value_type;

    inline bool empty() const { return m_elements.empty(); }

    inline behavior& back() { return *m_elements.back(); }

    // executes the behavior stack
    void exec();

    void pop_back();

    void push_back(behavior* what, bool has_ownership);

    void cleanup();

    void clear();

 private:

    std::vector<value_type> m_elements;
    std::vector<value_type> m_erased_elements;

};

} } // namespace cppa::detail

#endif // BEHAVIOR_STACK_HPP
