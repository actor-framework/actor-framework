/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_POLICY_ITERATIVE_STEALING_HPP
#define CAF_POLICY_ITERATIVE_STEALING_HPP

#include <cstddef>

#include "caf/fwd.hpp"

namespace caf {
namespace policy {

/**
 * @brief An implementation of the {@link steal_policy} concept
 *    that iterates over all other workers when stealing.
 *
 * @relates steal_policy
 */
class iterative_stealing {

 public:

  constexpr iterative_stealing() : m_victim(0) { }

  template <class Worker>
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
} // namespace caf

#endif // CAF_POLICY_ITERATIVE_STEALING_HPP
