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

#ifndef CAF_DOWNSTREAM_POLICY_HPP
#define CAF_DOWNSTREAM_POLICY_HPP

#include <cstddef>

#include "caf/fwd.hpp"

namespace caf {

/// Implements dispatching to downstream paths.
class downstream_policy {
public:
  virtual ~downstream_policy();

  /// Queries the optimal amount of data for the next `push` operation to `x`.
  virtual size_t available_credit(const abstract_downstream& x) = 0;

  /// Pushes data to the downstream paths of `x`, optionally passing the last
  /// result of `available_credit` for `x` as second argument.
  virtual void push(abstract_downstream& x, size_t* hint = nullptr) = 0;

  // TODO: callbacks für new and closed downstream pahts

  /// Reclaim credit of closed downstream `y`.
  //virtual void reclaim(abstract_downstream& x, downstream_path& y) = 0;
};

} // namespace caf

#endif // CAF_DOWNSTREAM_POLICY_HPP
