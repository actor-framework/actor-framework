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

#include "caf/serializer.hpp"

#include "caf/actor_system.hpp"

namespace caf {

serializer::serializer(actor_system& sys) noexcept
  : context_(sys.dummy_execution_unit()) {
  // nop
}

serializer::serializer(execution_unit* ctx) noexcept : context_(ctx) {
  // nop
}

serializer::~serializer() {
  // nop
}

auto serializer::apply(const std::vector<bool>& xs) -> result_type {
  if (auto err = begin_sequence(xs.size()))
    return err;
  for (bool value : xs)
    if (auto err = apply(value))
      return err;
  return end_sequence();
}

} // namespace caf
