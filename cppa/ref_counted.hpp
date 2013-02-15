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


#ifndef CPPA_REF_COUNTED_HPP
#define CPPA_REF_COUNTED_HPP

#include <atomic>
#include <cstddef>

#include "cppa/memory_managed.hpp"

namespace cppa {

/**
 * @brief A (thread safe) base class for reference counted objects
 *        with an atomic reference count.
 *
 * Serves the requirements of {@link intrusive_ptr}.
 * @relates intrusive_ptr
 */
class ref_counted : public memory_managed {

 public:

    ref_counted();

    /**
     * @brief Increases reference count by one.
     */
    inline void ref() { ++m_rc; }

    /**
     * @brief Decreases reference count by one and calls
     *        @p request_deletion when it drops to zero.
     */
    inline void deref() { if (--m_rc == 0) request_deletion(); }

    /**
     * @brief Queries whether there is exactly one reference.
     */
    inline bool unique() { return m_rc == 1; }

    inline size_t get_reference_count() const { return m_rc; }

 private:

    std::atomic<size_t> m_rc;

};

} // namespace cppa

#endif // CPPA_REF_COUNTED_HPP
