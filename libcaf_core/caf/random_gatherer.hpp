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

#ifndef CAF_RANDOM_GATHERER_HPP
#define CAF_RANDOM_GATHERER_HPP

#include "caf/stream_gatherer_impl.hpp"

namespace caf {

/// Pulls data from sources in arbitrary order.
class random_gatherer : public stream_gatherer_impl {
public:
  using super = stream_gatherer_impl;

  random_gatherer(local_actor* selfptr);

  ~random_gatherer() override;

  void assign_credit(long downstream_capacity) override;

  long initial_credit(long downstream_capacity, path_type* x) override;
};

} // namespace caf

#endif // CAF_RANDOM_GATHERER_HPP
