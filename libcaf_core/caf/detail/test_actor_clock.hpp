/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_TEST_ACTOR_CLOCK_HPP
#define CAF_DETAIL_TEST_ACTOR_CLOCK_HPP

#include "caf/detail/simple_actor_clock.hpp"

namespace caf {
namespace detail {

class test_actor_clock : public simple_actor_clock {
public:
  time_point current_time;

  time_point now() const noexcept override;

  void advance_time(duration_type x);
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TEST_ACTOR_CLOCK_HPP
