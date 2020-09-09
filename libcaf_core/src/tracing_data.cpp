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

namespace {

template <class Serializer>
bool serialize_impl(Serializer& sink, const tracing_data_ptr& x) {
  if (!x) {
    return sink.begin_object("tracing_data")   //
           && sink.begin_field("value", false) //
           && sink.end_field()                 //
           && sink.end_object();
  }
  return sink.begin_object("tracing_data")  //
         && sink.begin_field("value", true) //
         && x->serialize(sink)              //
         && sink.end_field()                //
         && sink.end_object();
}

template <class Deserializer>
bool deserialize_impl(Deserializer& source, tracing_data_ptr& x) {
  bool is_present = false;
  if (!source.begin_object("tracing_data")
      || !source.begin_field("value", is_present))
    return false;
  if (!is_present)
    return source.end_field() && source.end_object();
  auto ctx = source.context();
  if (ctx == nullptr) {
    source.emplace_error(sec::no_context,
                         "cannot deserialize tracing data without context");
    return false;
  }
  auto tc = ctx->system().tracing_context();
  if (tc == nullptr) {
    source.emplace_error(sec::no_tracing_context,
                         "cannot deserialize tracing data without context");
    return false;
  }
  return tc->deserialize(source, x) //
         && source.end_field()      //
         && source.end_object();
}

} // namespace

bool inspect(serializer& sink, const tracing_data_ptr& x) {
  return serialize_impl(sink, x);
}

bool inspect(binary_serializer& sink, const tracing_data_ptr& x) {
  return serialize_impl(sink, x);
}

bool inspect(deserializer& source, tracing_data_ptr& x) {
  return deserialize_impl(source, x);
}

bool inspect(binary_deserializer& source, tracing_data_ptr& x) {
  return deserialize_impl(source, x);
}

} // namespace caf
