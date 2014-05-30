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
        done,
        shutdown_execution_unit
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
