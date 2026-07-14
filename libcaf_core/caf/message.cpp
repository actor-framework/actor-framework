// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/message.hpp"

#include "caf/actor_system.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/message_builder.hpp"
#include "caf/message_handler.hpp"
#include "caf/serializer.hpp"
#include "caf/string_algorithms.hpp"

#include <utility>

#define GUARDED(statement)                                                     \
  if (!(statement))                                                            \
  return false

namespace caf {

bool message::load(deserializer& source) {
  type_id_list types{nullptr};
  GUARDED(source.begin_object(type_id_v<message>, "message") //
          && source.begin_field("types")                     //
          && source.value(types)                             //
          && source.end_field());
  GUARDED(source.begin_field("values") //
          && source.begin_tuple(types.size()));
  if (types.empty()) {
    data_.reset();
  } else {
    size_t data_size = 0;
    for (auto id : types) {
      if (const auto* meta = detail::global_meta_object_or_null(id)) {
        data_size += meta->padded_size;
      } else {
        auto msg = detail::format("unknown type ID: {}",
                                  detail::to_underlying(id));
        source.emplace_error(sec::unknown_type, std::move(msg));
        return false;
      }
    }
    intrusive_ptr<detail::message_data> ptr;
    if (auto vptr = malloc(sizeof(detail::message_data) + data_size)) {
      // We don't need to worry about exceptions here: the message_data
      // constructor is `noexcept`.
      ptr.reset(new (vptr) detail::message_data(types), adopt_ref);
    } else {
      source.emplace_error(sec::runtime_error, "unable to allocate memory");
      return false;
    }
    auto pos = ptr->storage();
    auto msg_types = ptr->types();
    auto gmos = detail::global_meta_objects();
    for (size_t i = 0; i < types.size(); ++i) {
      auto& meta = gmos[detail::to_underlying(msg_types[i])];
      meta.default_construct(pos);
      ptr->inc_constructed_elements();
      if (!meta.load(source, pos))
        return false;
      pos += meta.padded_size;
    }
    data_.reset(ptr.release(), adopt_ref);
  }
  return source.end_tuple() && source.end_field() && source.end_object();
}

bool message::save(serializer& sink) const {
  auto gmos = detail::global_meta_objects();
  if (data_ == nullptr) {
    // Short-circuit empty sequences.
    return sink.begin_object(type_id_v<message>, "message") //
           && sink.begin_field("types")                     //
           && sink.value(make_type_id_list())               //
           && sink.end_field()                              //
           && sink.begin_field("values")                    //
           && sink.begin_tuple(0)                           //
           && sink.end_tuple()                              //
           && sink.end_field()                              //
           && sink.end_object();
  }
  GUARDED(sink.begin_object(type_id_v<message>, "message"));
  auto type_ids = data_->types();
  // Write type information. This is needed for serialization round trips. Human
  // readable formats will have human readable type information.
  auto ok = sink.begin_field("types")     //
            && sink.value(type_ids)       //
            && sink.end_field()           //
            && sink.begin_field("values") //
            && sink.begin_tuple(type_ids.size());
  if (!ok) {
    return false;
  }
  auto storage = data_->storage();
  for (auto id : type_ids) {
    auto& meta = gmos[detail::to_underlying(id)];
    GUARDED(meta.save(sink, storage));
    storage += meta.padded_size;
  }
  return sink.end_tuple() && sink.end_field() && sink.end_object();
}

bool message::save(detail::stringification_inspector& sink) const {
  auto str = to_string(*this);
  return sink.value(str);
}

// -- related non-members ------------------------------------------------------

std::string to_string(const message& msg) {
  if (msg.empty())
    return "message()";
  std::string result;
  result += "message(";
  auto types = msg.types();
  if (!types.empty()) {
    auto ptr = msg.cdata().storage();
    auto* meta = &detail::global_meta_object(types[0]);
    meta->stringify(result, ptr);
    ptr += meta->padded_size;
    for (size_t index = 1; index < types.size(); ++index) {
      result += ", ";
      meta = &detail::global_meta_object(types[index]);
      meta->stringify(result, ptr);
      ptr += meta->padded_size;
    }
  }
  result += ')';
  return result;
}

} // namespace caf
