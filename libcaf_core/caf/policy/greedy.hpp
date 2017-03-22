/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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
  greedy();

  void assign_credit(assignment_vec& xs, size_t buf_size,
                     size_t downstream_credit) override;

  size_t low_watermark;

  size_t high_watermark;

  size_t min_buffer_size;
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_GREEDY_HPP
