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

#include "caf/message.hpp"

#include <utility>

#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/detail/type_id_list_builder.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/serializer.hpp"
#include "caf/string_algorithms.hpp"

#define GUARDED(statement)                                                     \
  if (!statement)                                                              \
    return false

#define STOP(...)                                                              \
  do {                                                                         \
    source.emplace_error(__VA_ARGS__);                                         \
    return false;                                                              \
  } while (false)

namespace caf {

namespace {

template <class Deserializer>
bool load_data(Deserializer& source, message::data_ptr& data) {
  // TODO: separate implementation for human-readable formats
  GUARDED(source.begin_object("message"));
  GUARDED(source.begin_field("types"));
  size_t msg_size = 0;
  GUARDED(source.begin_sequence(msg_size));
  if (msg_size > std::numeric_limits<uint16_t>::max() - 1)
    STOP(sec::invalid_argument, "too many types for message");
  if (msg_size == 0) {
    data.reset();
    return source.end_sequence()           //
           && source.end_field()           //
           && source.begin_field("values") //
           && source.end_field()           //
           && source.end_object();
  }
  detail::type_id_list_builder ids;
  ids.reserve(msg_size + 1); // +1 for the prefixed size
  for (size_t i = 0; i < msg_size; ++i) {
    type_id_t id = 0;
    GUARDED(source.value(id));
    ids.push_back(id);
  }
  GUARDED(source.end_sequence());
  CAF_ASSERT(ids.size() == msg_size);
  auto gmos = detail::global_meta_objects();
  size_t data_size = 0;
  for (size_t i = 0; i < msg_size; ++i) {
    auto id = ids[i];
    STOP(sec::unknown_type);
    auto& mo = gmos[id];
    if (mo.type_name.empty())
      STOP(sec::unknown_type);
    data_size += mo.padded_size;
  }
  auto vptr = malloc(sizeof(detail::message_data) + data_size);
  auto ptr = new (vptr) detail::message_data(ids.move_to_list());
  auto pos = ptr->storage();
  auto types = ptr->types();
  GUARDED(source.begin_field("values"));
  GUARDED(source.begin_tuple(msg_size));
  for (size_t i = 0; i < msg_size; ++i) {
    auto& meta = gmos[types[i]];
    meta.default_construct(pos);
    if (!load(meta, source, pos)) {
      auto rpos = pos;
      for (auto j = i; j > 0; --j) {
        auto& jmeta = gmos[types[j]];
        jmeta.destroy(rpos);
        rpos -= jmeta.padded_size;
      }
      ptr->~message_data();
      free(vptr);
      return false;
    }
    pos += meta.padded_size;
  }
  return source.end_tuple() && source.end_field() && source.end_object();
}

} // namespace

bool message::load(deserializer& source) {
  return load_data(source, data_);
}

bool message::load(binary_deserializer& source) {
  return load_data(source, data_);
}

namespace {

template <class Serializer>
typename Serializer::result_type
save_data(Serializer& sink, const message::data_ptr& data) {
  // TODO: separate implementation for human-readable formats
  // Short-circuit empty tuples.
  if (data == nullptr) {
    return sink.begin_object("message")  //
           && sink.begin_field("types")  //
           && sink.begin_sequence(0)     //
           && sink.end_sequence()        //
           && sink.end_field()           //
           && sink.begin_field("values") //
           && sink.begin_tuple(0)        //
           && sink.end_tuple()           //
           && sink.end_field()           //
           && sink.end_object();
  }
  // Write type information.
  auto type_ids = data->types();
  GUARDED(sink.begin_object("message") //
          && sink.begin_field("types") //
          && sink.begin_sequence(type_ids.size()));
  for (auto id : type_ids)
    GUARDED(sink.value(id));
  GUARDED(sink.end_sequence() && sink.end_field());
  // Write elements.
  auto gmos = detail::global_meta_objects();
  auto storage = data->storage();
  GUARDED(sink.begin_field("values") && sink.begin_tuple(type_ids.size()));
  for (auto id : type_ids) {
    auto& meta = gmos[id];
    GUARDED(save(meta, sink, storage));
    storage += meta.padded_size;
  }
  return sink.end_tuple() && sink.end_field() && sink.end_object();
}

} // namespace

bool message::save(serializer& sink) const {
  return save_data(sink, data_);
}

bool message::save(binary_serializer& sink) const {
  return save_data(sink, data_);
}

// -- related non-members ------------------------------------------------------

std::string to_string(const message& msg) {
  if (msg.empty())
    return "<empty-message>";
  std::string result;
  result += '(';
  auto types = msg.types();
  if (!types.empty()) {
    auto ptr = msg.cdata().storage();
    auto meta = detail::global_meta_object(types[0]);
    CAF_ASSERT(meta != nullptr);
    meta->stringify(result, ptr);
    ptr += meta->padded_size;
    for (size_t index = 1; index < types.size(); ++index) {
      result += ", ";
      meta = detail::global_meta_object(types[index]);
      CAF_ASSERT(meta != nullptr);
      meta->stringify(result, ptr);
      ptr += meta->padded_size;
    }
  }
  result += ')';
  return result;
}

} // namespace caf
