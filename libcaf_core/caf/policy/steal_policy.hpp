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

#ifndef CAF_POLICY_STEAL_POLICY_HPP
#define CAF_POLICY_STEAL_POLICY_HPP

#include <cstddef>

#include "caf/fwd.hpp"

namespace caf {
namespace policy {

/**
 * @brief This concept class describes the interface of a policy class
 *    for stealing jobs from other workers.
 */
class steal_policy {

 public:

  /**
   * @brief Go on a raid in quest for a shiny new job. Returns @p nullptr
   *    if no other worker provided any work to steal.
   */
  template <class Worker>
  resumable* raid(Worker* self);

};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_STEAL_POLICY_HPP
