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


#ifndef CPPA_MESSAGE_QUEUE_HPP
#define CPPA_MESSAGE_QUEUE_HPP

#include "cppa/any_tuple.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/network/message_header.hpp"

namespace cppa { namespace network {

class default_message_queue : public ref_counted {

 public:

    typedef std::pair<message_header,any_tuple> value_type;

    typedef value_type& reference;

    template<typename... Args>
    void emplace(Args&&... args) {
        m_impl.emplace_back(std::forward<Args>(args)...);
    }

    inline bool empty() const { return m_impl.empty(); }

    inline value_type pop() {
        value_type result(std::move(m_impl.front()));
        m_impl.erase(m_impl.begin());
        return std::move(result);
    }

 private:

    std::vector<value_type> m_impl;

};

typedef intrusive_ptr<default_message_queue> default_message_queue_ptr;

} } // namespace cppa::network



#endif // CPPA_MESSAGE_QUEUE_HPP
