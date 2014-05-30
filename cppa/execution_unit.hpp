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


#ifndef CPPA_DETAIL_EXECUTION_UNIT_HPP
#define CPPA_DETAIL_EXECUTION_UNIT_HPP

namespace cppa {

class resumable;

/*
 * @brief Identifies an execution unit, e.g., a worker thread of the scheduler.
 */
class execution_unit {

 public:

    virtual ~execution_unit();

    /*
     * @brief Enqueues @p ptr to the job list of the execution unit.
     * @warning Must only be called from a {@link resumable} currently
     *          executed by this execution unit.
     */
    virtual void exec_later(resumable* ptr) = 0;

};

} // namespace cppa

#endif // CPPA_DETAIL_EXECUTION_UNIT_HPP
