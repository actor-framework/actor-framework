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


#ifndef CPPA_WEAK_INTRUSIVE_PTR_HPP
#define CPPA_WEAK_INTRUSIVE_PTR_HPP

#include <cstddef>

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/weak_ptr_anchor.hpp"
#include "cppa/util/comparable.hpp"

namespace cppa {

/**
 * @brief A smart pointer that does not increase the reference count.
 */
template<typename T>
class weak_intrusive_ptr : util::comparable<weak_intrusive_ptr<T>> {

 public:

    weak_intrusive_ptr(const intrusive_ptr<T>& from) {
        if (from) m_anchor = from->get_weak_ptr_anchor();
    }

    weak_intrusive_ptr() = default;
    weak_intrusive_ptr(const weak_intrusive_ptr&) = default;
    weak_intrusive_ptr& operator=(const weak_intrusive_ptr&) = default;

    /**
     * @brief Promotes this weak pointer to an intrusive_ptr.
     * @warning Returns @p nullptr if {@link expired()}.
     */
    intrusive_ptr<T> promote() {
        return (m_anchor) ? m_anchor->get<T>() : nullptr;
    }

    /**
     * @brief Queries whether the object was already deleted.
     */
    bool expired() const {
        return (m_anchor) ? m_anchor->expired() : true;
    }

    inline ptrdiff_t compare(const weak_intrusive_ptr& other) const {
        return m_anchor.compare(other.m_anchor);
    }

    /**
     * @brief Queries whether this weak pointer is invalid, i.e., does not
     *        point to an object.
     */
    inline bool invalid() const {
        return m_anchor == nullptr;
    }

 private:

    intrusive_ptr<weak_ptr_anchor> m_anchor;

};

} // namespace cppa

#endif // CPPA_WEAK_INTRUSIVE_PTR_HPP
