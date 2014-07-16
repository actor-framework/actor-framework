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

#ifndef CAF_BEHAVIOR_POLICY_HPP
#define CAF_BEHAVIOR_POLICY_HPP

namespace caf {

template<bool DiscardBehavior>
struct behavior_policy {
    static constexpr bool discard_old = DiscardBehavior;

};

template<typename T>
struct is_behavior_policy : std::false_type {};

template<bool DiscardBehavior>
struct is_behavior_policy<behavior_policy<DiscardBehavior>> : std::true_type {};

using keep_behavior_t = behavior_policy<false>;
using discard_behavior_t = behavior_policy<true>;

/**
 * @brief Policy tag that causes {@link event_based_actor::become} to
 *        discard the current behavior.
 * @relates local_actor
 */
constexpr discard_behavior_t discard_behavior = discard_behavior_t{};

/**
 * @brief Policy tag that causes {@link event_based_actor::become} to
 *        keep the current behavior available.
 * @relates local_actor
 */
constexpr keep_behavior_t keep_behavior = keep_behavior_t{};

} // namespace caf

#endif // CAF_BEHAVIOR_POLICY_HPP
