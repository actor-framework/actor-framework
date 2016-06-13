/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_POLICY_GREEDY_HPP
#define CAF_POLICY_GREEDY_HPP

#include "caf/upstream_policy.hpp"

namespace caf {
namespace policy {

/// Sends ACKs as early and often as possible.
class greedy final : public upstream_policy {
public:
  size_t initial_credit(abstract_upstream& x, upstream_path& y) override;

  void reclaim(abstract_upstream& x, upstream_path& y) override;
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_GREEDY_HPP
