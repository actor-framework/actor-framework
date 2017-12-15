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

#include "caf/instrumentation/worker_stats.hpp"

#include <cmath>
#include <array>
#include <string>
#include <cstdint>
#include <utility>

namespace caf {
namespace instrumentation {

struct metric {
  metric(std::string actor, std::string callsite, const callsite_stats& value)
    : actortype(std::move(actor)),
      callsite(std::move(callsite)),
      value(value)
  {}

  std::string actortype;
  std::string callsite;
  callsite_stats value;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_METRIC_HPP
