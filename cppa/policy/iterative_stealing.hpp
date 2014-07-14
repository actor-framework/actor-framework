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

#ifndef CPPA_POLICY_ITERATIVE_STEALING_HPP
#define CPPA_POLICY_ITERATIVE_STEALING_HPP

#include <cstddef>

#include "cppa/fwd.hpp"

namespace cppa {
namespace policy {

/**
 * @brief An implementation of the {@link steal_policy} concept
 *        that iterates over all other workers when stealing.
 *
 * @relates steal_policy
 */
class iterative_stealing {

 public:

    constexpr iterative_stealing() : m_victim(0) { }

    template<class Worker>
    resumable* raid(Worker* self) {
        // try once to steal from anyone
        auto inc = [](size_t arg) -> size_t {
            return arg + 1;
        };
        auto dec = [](size_t arg) -> size_t {
            return arg - 1;
        };
        // reduce probability of 'steal collisions' by letting
        // half the workers pick victims by increasing IDs and
        // the other half by decreasing IDs
        size_t (*next)(size_t) = (self->id() % 2) == 0 ? inc : dec;
        auto n = self->parent()->num_workers();
        for (size_t i = 0; i < n; ++i) {
            m_victim = next(m_victim) % n;
            if (m_victim != self->id()) {
                auto job = self->parent()->worker_by_id(m_victim)->try_steal();
                if (job) {
                    return job;
                }
            }
        }
        return nullptr;
    }

 private:

    size_t m_victim;

};

} // namespace policy
} // namespace cppa

#endif // CPPA_POLICY_ITERATIVE_STEALING_HPP
