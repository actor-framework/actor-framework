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

#ifndef CAF_UPSTREAM_POLICY_HPP
#define CAF_UPSTREAM_POLICY_HPP

#include <cstdint>

#include "caf/fwd.hpp"

namespace caf {

class upstream_policy {
public:
  virtual ~upstream_policy();

  /// Queries the initial credit for the new path `y` in `x`.
  virtual size_t initial_credit(abstract_upstream& x, upstream_path& y) = 0;

  /// Reclaim credit of closed upstream `y`.
  virtual void reclaim(abstract_upstream& x, upstream_path& y) = 0;
};

} // namespace caf

#endif // CAF_UPSTREAM_POLICY_HPP
