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

#ifndef CPPA_POLICY_STEAL_POLICY_HPP
#define CPPA_POLICY_STEAL_POLICY_HPP

#include <cstddef>

#include "cppa/fwd.hpp"

namespace cppa {
namespace policy {

/**
 * @brief This concept class describes the interface of a policy class
 *        for stealing jobs from other workers.
 */
class steal_policy {

 public:

    /**
     * @brief Go on a raid in quest for a shiny new job. Returns @p nullptr
     *        if no other worker provided any work to steal.
     */
    template<class Worker>
    resumable* raid(Worker* self);

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_STEAL_POLICY_HPP
