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

#ifndef CAF_PULL5_GATHERER_HPP
#define CAF_PULL5_GATHERER_HPP

#include "caf/random_gatherer.hpp"

namespace caf {
namespace detail {

/// Always pulls exactly 5 elements from sources. Used in unit tests only.
class pull5_gatherer : public random_gatherer {
public:
  using super = random_gatherer;

  pull5_gatherer(local_actor* selfptr);

  void assign_credit(long available) override;

  long initial_credit(long, inbound_path*) override;
};

} // namespace detail
} // namespace caf

#endif // CAF_PULL5_GATHERER_HPP
