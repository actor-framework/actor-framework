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

#ifndef CAF_UPSTREAM_POLICY_HPP
#define CAF_UPSTREAM_POLICY_HPP

#include <vector>
#include <cstdint>
#include <utility>

#include "caf/fwd.hpp"

namespace caf {

class upstream_policy {
public:
  virtual ~upstream_policy();

  /// Describes an assignment of credit to an upstream actor.
  using assignment_pair = std::pair<upstream_path*, long>;

  /// Describes an assignment of credit to all upstream actors.
  using assignment_vec = std::vector<assignment_pair>;

  /// Assigns credit to upstream actors.
  /// @param xs Stores assignment decisions. Note that the second element of
  ///           each pair is uninitialized and must be set to 0 for all paths
  ///           that do not receive credit.
  /// @param total_downstream_net_credit Denotes how many items we could send
  ///                                    downstream. A negative value indicates
  ///                                    that we are already buffering more
  ///                                    items than we can send.
  virtual void assign_credit(assignment_vec& xs,
                             long total_downstream_net_credit) = 0;
};

} // namespace caf

#endif // CAF_UPSTREAM_POLICY_HPP
