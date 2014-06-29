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

#ifndef CPPA_DETAIL_HANDLE_HPP
#define CPPA_DETAIL_HANDLE_HPP

#include <cstdint>

#include "cppa/detail/comparable.hpp"

namespace cppa {

/**
 * @brief Base class for IO handles such as {@link accept_handle} or
 *        {@link connection_handle}.
 */
template<typename Subtype, int64_t InvalidId = -1>
class io_handle : detail::comparable<Subtype> {

 public:

    constexpr io_handle() : m_id{InvalidId} {}

    io_handle(const Subtype& other) { m_id = other.id(); }

    io_handle(const io_handle& other) = default;

    Subtype& operator=(const io_handle& other) {
        m_id = other.id();
        return *static_cast<Subtype*>(this);
    }

    /**
     * @brief Returns the unique identifier of this handle.
     */
    inline int64_t id() const { return m_id; }

    /**
     * @brief Sets the unique identifier of this handle.
     */
    inline void set_id(int64_t value) { m_id = value; }

    inline int64_t compare(const Subtype& other) const {
        return m_id - other.id();
    }

    inline bool invalid() const { return m_id == -1; }

    static inline Subtype from_int(int64_t id) {
        return {id};
    }

 protected:

    inline io_handle(int64_t handle_id) : m_id{handle_id} {}

 private:

    int64_t m_id;

};

} // namespace cppa

#endif // CPPA_DETAIL_HANDLE_HPP
