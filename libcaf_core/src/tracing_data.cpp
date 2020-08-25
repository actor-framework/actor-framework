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
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
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

template <class Inspector>
bool inspect(Inspector& f, tracing_data& x) {
  return x.serialize(f);
}

namespace {

using access = optional_inspector_access<tracing_data_ptr>;

} // namespace

bool inspect(serializer& sink, const tracing_data_ptr& x) {
  return access::apply_object(sink, detail::as_mutable_ref(x));
}

bool inspect(binary_serializer& sink, const tracing_data_ptr& x) {
  return access::apply_object(sink, detail::as_mutable_ref(x));
}

bool inspect(deserializer& source, tracing_data_ptr& x) {
  return access::apply_object(source, x);
}

bool inspect(binary_deserializer& source, tracing_data_ptr& x) {
  return access::apply_object(source, x);
}

} // namespace caf
