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

#ifndef CAF_BEHAVIOR_POLICY_HPP
#define CAF_BEHAVIOR_POLICY_HPP

namespace caf {

template <bool DiscardBehavior>
struct behavior_policy {
  static constexpr bool discard_old = DiscardBehavior;

};

template <class T>
struct is_behavior_policy : std::false_type {};

template <bool DiscardBehavior>
struct is_behavior_policy<behavior_policy<DiscardBehavior>> : std::true_type {};

using keep_behavior_t = behavior_policy<false>;
using discard_behavior_t = behavior_policy<true>;

/**
 * Policy tag that causes {@link event_based_actor::become} to
 * discard the current behavior.
 * @relates local_actor
 */
constexpr discard_behavior_t discard_behavior = discard_behavior_t{};

/**
 * Policy tag that causes {@link event_based_actor::become} to
 * keep the current behavior available.
 * @relates local_actor
 */
constexpr keep_behavior_t keep_behavior = keep_behavior_t{};

} // namespace caf

#endif // CAF_BEHAVIOR_POLICY_HPP
