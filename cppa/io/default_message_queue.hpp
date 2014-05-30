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
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/


#ifndef CPPA_IO_DEFAULT_MESSAGE_QUEUE_HPP
#define CPPA_IO_DEFAULT_MESSAGE_QUEUE_HPP

#include <vector>

#include "cppa/any_tuple.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/message_header.hpp"

namespace cppa {
namespace io {

class default_message_queue : public ref_counted {

 public:

    typedef std::pair<message_header, any_tuple> value_type;

    typedef value_type& reference;

    ~default_message_queue();

    template<typename... Ts>
    void emplace(Ts&&... args) {
        m_impl.emplace_back(std::forward<Ts>(args)...);
    }

    inline bool empty() const { return m_impl.empty(); }

    inline value_type pop() {
        value_type result(std::move(m_impl.front()));
        m_impl.erase(m_impl.begin());
        return result;
    }

 private:

    std::vector<value_type> m_impl;

};

typedef intrusive_ptr<default_message_queue> default_message_queue_ptr;

} // namespace io
} // namespace cppa

#endif // CPPA_IO_DEFAULT_MESSAGE_QUEUE_HPP
