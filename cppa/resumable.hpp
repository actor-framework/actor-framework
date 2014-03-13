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


#ifndef CPPA_RESUMABLE_HPP
#define CPPA_RESUMABLE_HPP

namespace cppa {

class execution_unit;

namespace detail { struct cs_thread; }

/**
 * @brief A cooperatively executed task managed by one or more instances of
 *        {@link execution_unit}.
 */
class resumable {

 public:

    enum resume_result {
        resume_later,
        done
    };

    resumable();

    virtual ~resumable();

    /**
     * @brief Initializes this object, e.g., by increasing the
     *        the reference count.
     */
    virtual void attach_to_scheduler() = 0;

    /**
     * @brief Uninitializes this object, e.g., by decrementing the
     *        the reference count.
     */
    virtual void detach_from_scheduler() = 0;

    /**
     * @brief Resume any pending computation until it is either finished
     *        or needs to be re-scheduled later.
     */
    virtual resume_result resume(detail::cs_thread*, execution_unit*) = 0;

 protected:

    bool m_hidden;

};

} // namespace cppa

#endif // CPPA_RESUMABLE_HPP
