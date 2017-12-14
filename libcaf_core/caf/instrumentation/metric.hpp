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

#ifndef CAF_METRIC_HPP
#define CAF_METRIC_HPP

#include <cmath>
#include <array>
#include <string>
#include <cstdint>
#include <utility>

namespace caf {
namespace instrumentation {

struct metric {
  metric(std::string location, std::string name, uint64_t value)
    : location(std::move(location)),
      name(std::move(name)),
      value(value)
  {}

  std::string location;
  std::string name;
  uint64_t value;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_METRIC_HPP
