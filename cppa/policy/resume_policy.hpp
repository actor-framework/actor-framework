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


#ifndef CPPA_RESUME_POLICY_HPP
#define CPPA_RESUME_POLICY_HPP

#include "cppa/resumable.hpp"

// this header consists all type definitions needed to
// implement the resume_policy trait

namespace cppa { namespace util { class duration; struct fiber; } }

namespace cppa { namespace policy {

/**
 * @brief The resume_policy <b>concept</b> class. Please note that this
 *        class is <b>not</b> implemented. It only explains the all
 *        required member function and their behavior for any resume policy.
 */
class resume_policy {

 public:

    /**
     * @brief Resumes the actor by reading a new message <tt>msg</tt> and then
     *        calling <tt>self->invoke(msg)</tt>. This process is repeated
     *        until either no message is left in the actor's mailbox or the
     *        actor finishes execution.
     */
    template<class Actor>
    resumable::resume_result resume(Actor* self, util::fiber* from);

    /**
     * @brief Waits unconditionally until a new message arrives.
     * @note This member function must raise a compiler error if the resume
     *       strategy cannot be used to implement blocking actors.
     */
    template<class Actor>
    bool await_data(Actor* self);

    /**
     * @brief Waits until either a new message arrives, or a timeout occurs.
     *        The @p abs_time argument is the return value of
     *        {@link scheduling_policy::init_timeout}. Returns true if a
     *        message arrived in time, otherwise false.
     * @note This member function must raise a compiler error if the resume
     *       strategy cannot be used to implement blocking actors.
     */
    template<class Actor, typename AbsTimeout>
    bool await_data(Actor* self, const AbsTimeout& abs_time);

};

} } // namespace cppa::policy

#endif // CPPA_RESUME_POLICY_HPP
