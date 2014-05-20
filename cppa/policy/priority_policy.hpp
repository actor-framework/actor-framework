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


#ifndef CPPA_PRIORITY_POLICY_HPP
#define CPPA_PRIORITY_POLICY_HPP

namespace cppa { class mailbox_element; }

namespace cppa {
namespace policy {

/**
 * @brief The priority_policy <b>concept</b> class. Please note that this
 *        class is <b>not</b> implemented. It only explains the all
 *        required member function and their behavior for any priority policy.
 */
class priority_policy {

 public:

    /**
     * @brief Returns the next message from the mailbox or @p nullptr
     *        if it's empty.
     */
    template<class Actor>
    unique_mailbox_element_pointer next_message(Actor* self);

    /**
     * @brief Queries whether the mailbox is not empty.
     */
    template<class Actor>
    bool has_next_message(Actor* self);

    void push_to_cache(unique_mailbox_element_pointer ptr);

    typedef std::vector<unique_mailbox_element_pointer> cache_type;

    typedef cache_type::iterator cache_iterator;

    cache_iterator cache_begin();

    cache_iterator cache_end();

    void cache_erase(cache_iterator iter);

};

} // namespace policy
} // namespace cppa


#endif // CPPA_PRIORITY_POLICY_HPP
