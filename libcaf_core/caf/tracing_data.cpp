// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/tracing_data.hpp"

#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/error.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"
#include "caf/serializer.hpp"
#include "caf/tracing_data_factory.hpp"

#include <cstdint>

namespace caf {

tracing_data::~tracing_data() {
  // nop
}

namespace {

template <class Serializer>
bool serialize_impl(Serializer& sink, const tracing_data_ptr& x) {
  if (!x) {
    return sink.begin_object(invalid_type_id, "tracing_data") //
           && sink.begin_field("value", false)                //
           && sink.end_field()                                //
           && sink.end_object();
  }
  return sink.begin_object(invalid_type_id, "tracing_data") //
         && sink.begin_field("value", true)                 //
         && x->serialize(sink)                              //
         && sink.end_field()                                //
         && sink.end_object();
}

template <class Deserializer>
bool deserialize_impl(Deserializer& source, tracing_data_ptr& x) {
  bool is_present = false;
  if (!source.begin_object(invalid_type_id, "tracing_data")
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
