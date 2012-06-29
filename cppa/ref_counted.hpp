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


#ifndef CPPA_REF_COUNTED_HPP
#define CPPA_REF_COUNTED_HPP

#include <atomic>
#include <cstddef>

#include "cppa/detail/ref_counted_impl.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief A (thread safe) base class for reference counted objects
 *        with an atomic reference count.
 *
 * Serves the requirements of {@link intrusive_ptr}.
 * @relates intrusive_ptr
 */
class ref_counted {

 public:

    /**
     * @brief Increases reference count by one.
     */
    void ref();

    /**
     * @brief Decreases reference cound by one.
     * @returns @p true if there are still references to this object
     *          (reference count > 0); otherwise @p false.
     */
    bool deref();

    /**
     * @brief Queries if there is exactly one reference.
     * @returns @p true if reference count is one; otherwise @p false.
     */
    bool unique();

};

#else

typedef detail::ref_counted_impl< std::atomic<size_t> > ref_counted;

#endif

} // namespace cppa

#endif // CPPA_REF_COUNTED_HPP
