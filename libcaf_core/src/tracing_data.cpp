/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/tracing_data.hpp"

#include <cstdint>

#include "caf/actor_system.hpp"
#include "caf/deserializer.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"
#include "caf/tracing_data_factory.hpp"

namespace caf {

tracing_data::~tracing_data() {
  // nop
}

error inspect(serializer& sink, const tracing_data_ptr& x) {
  if (x == nullptr) {
    uint8_t dummy = 0;
    return sink(dummy);
  }
  uint8_t dummy = 1;
  if (auto err = sink(dummy))
    return err;
  return x->serialize(sink);
}

error inspect(deserializer& source, tracing_data_ptr& x) {
  uint8_t dummy = 0;
  if (auto err = source(dummy))
    return err;
  if (dummy == 0) {
    x.reset();
    return none;
  }
  auto ctx = source.context();
  if (ctx == nullptr)
    return sec::no_context;
  auto tc = ctx->system().tracing_context();
  if (tc == nullptr)
    return sec::no_tracing_context;
  return tc->deserialize(source, x);
}

} // namespace caf
