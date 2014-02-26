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


#ifndef CPPA_WEAK_PTR_ANCHOR_HPP
#define CPPA_WEAK_PTR_ANCHOR_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

namespace cppa {

/**
 * @brief A storage holding a spinlock and a pointer to a
 *        reference counted object.
 */
class weak_ptr_anchor : public ref_counted {

 public:

    weak_ptr_anchor(ref_counted* ptr);

    /**
     * @brief Gets a pointer to the object or nullptr if {@link expired()}.
     */
    template<typename T>
    intrusive_ptr<T> get() {
        intrusive_ptr<T> result;
        { // lifetime scope of guard
            util::shared_lock_guard<util::shared_spinlock> guard(m_lock);
            if (m_ptr) result.reset(static_cast<T*>(m_ptr));
        }
        return result;
    }

    /**
     * @brief Queries whether the object was already deleted.
     */
    inline bool expired() const {
        // no need for locking since pointer comparison is atomic
        return m_ptr == nullptr;
    }

    /**
     * @brief Tries to expire this anchor. Fails if reference count of object
     *        is not zero.
     */
    bool try_expire();

 private:

    ref_counted* m_ptr;
    util::shared_spinlock m_lock;

};

} // namespace cppa

#endif // CPPA_WEAK_PTR_ANCHOR_HPP
