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


#ifndef CPPA_CHANNEL_HPP
#define CPPA_CHANNEL_HPP

#include <cstddef>
#include <type_traits>

#include "cppa/intrusive_ptr.hpp"
#include "cppa/abstract_channel.hpp"

#include "cppa/util/comparable.hpp"

namespace cppa {

class actor;
class group;
class execution_unit;

struct invalid_actor_t;
struct invalid_group_t;

namespace detail { class raw_access; }

/**
 * @brief A handle to instances of {@link abstract_channel}.
 */
class channel : util::comparable<channel>
              , util::comparable<channel, actor>
              , util::comparable<channel, abstract_channel*> {

    friend class detail::raw_access;

 public:

    channel() = default;

    channel(const actor&);

    channel(const group&);

    channel(const invalid_actor_t&);

    channel(const invalid_group_t&);

    template<typename T>
    channel(intrusive_ptr<T> ptr,
            typename std::enable_if<
                std::is_base_of<abstract_channel, T>::value
            >::type* = 0)
        : m_ptr(ptr) { }

    channel(abstract_channel* ptr);

    inline explicit operator bool() const {
        return static_cast<bool>(m_ptr);
    }

    inline bool operator!() const {
        return !m_ptr;
    }

    inline abstract_channel* operator->() const {
        return m_ptr.get();
    }

    inline abstract_channel& operator*() const {
        return *m_ptr;
    }

    intptr_t compare(const channel& other) const;

    intptr_t compare(const actor& other) const;

    intptr_t compare(const abstract_channel* other) const;

    static intptr_t compare(const abstract_channel* lhs,
                            const abstract_channel* rhs);

 private:

    intrusive_ptr<abstract_channel> m_ptr;

};

} // namespace cppa

#endif // CPPA_CHANNEL_HPP
