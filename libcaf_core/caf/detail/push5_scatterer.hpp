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

#ifndef CAF_PUSH5_SCATTERER_HPP
#define CAF_PUSH5_SCATTERER_HPP

#include "caf/broadcast_scatterer.hpp"

namespace caf {
namespace detail {

/// Always pushs exactly 5 elements to sinks. Used in unit tests only.
template <class T>
class push5_scatterer : public broadcast_scatterer<T> {
public:
  using super = broadcast_scatterer<T>;

  push5_scatterer(local_actor* self) : super(self) {
    // nop
  }

  using super::min_batch_size;
  using super::max_batch_size;
  using super::min_buffer_size;

  long min_batch_size() const override {
    return 1;
  }

  long max_batch_size() const override {
    return 5;
  }

  long min_buffer_size() const override {
    return 5;
  }
};

} // namespace detail
} // namespace caf

#endif // CAF_PUSH5_SCATTERER_HPP
