// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
  if (!(statement))                                                            \
  return false

#define STOP(...)                                                              \
  do {                                                                         \
    source.emplace_error(__VA_ARGS__);                                         \
    return false;                                                              \
  } while (false)

namespace caf {

namespace {

bool load(const detail::meta_object& meta, caf::deserializer& source,
          void* obj) {
  return meta.load(source, obj);
}

bool load(const detail::meta_object& meta, caf::binary_deserializer& source,
          void* obj) {
  return meta.load_binary(source, obj);
}

template <class Deserializer>
bool load_data(Deserializer& source, message::data_ptr& data) {
  // For machine-to-machine data formats, we prefix the type information.
  if (!source.has_human_readable_format()) {
    GUARDED(source.begin_object(type_id_v<message>, "message"));
    GUARDED(source.begin_field("types"));
    size_t msg_size = 0;
    GUARDED(source.begin_sequence(msg_size));
    using uint16_limits = std::numeric_limits<uint16_t>;
    if (msg_size > static_cast<size_t>(uint16_limits::max() - 1))
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
    ids.reserve(msg_size);
    for (size_t i = 0; i < msg_size; ++i) {
      type_id_t id = 0;
      GUARDED(source.value(id));
      ids.push_back(id);
    }
    GUARDED(source.end_sequence());
    CAF_ASSERT(ids.size() == msg_size);
    size_t data_size = 0;
    for (auto id : ids) {
      if (auto meta_obj = detail::global_meta_object(id))
        data_size += meta_obj->padded_size;
      else
        STOP(sec::unknown_type);
    }
    intrusive_ptr<detail::message_data> ptr;
    if (auto vptr = malloc(sizeof(detail::message_data) + data_size)) {
      // We don't need to worry about exceptions here: the message_data
      // constructor as well as `move_to_list` are `noexcept`.
      ptr.reset(new (vptr) detail::message_data(ids.move_to_list()), false);
    } else {
      STOP(sec::runtime_error, "unable to allocate memory");
    }
    auto pos = ptr->storage();
    auto types = ptr->types();
    auto gmos = detail::global_meta_objects();
    GUARDED(source.begin_field("values"));
    GUARDED(source.begin_tuple(msg_size));
    for (size_t i = 0; i < msg_size; ++i) {
      auto& meta = gmos[types[i]];
      meta.default_construct(pos);
      ptr->inc_constructed_elements();
      if (!load(meta, source, pos))
        return false;
      pos += meta.padded_size;
    }
    data.reset(ptr.release(), false);
    return source.end_tuple() && source.end_field() && source.end_object();
  }
  // For human-readable data formats, we serialize messages as a single list of
  // dynamically-typed objects. This is more expensive, because we first need
  // the full type information before we can allocate the storage. Hence, we
  // must deserialize each element into a separate memory block first and then
  // move them to their final destination later.
  struct object_ptr {
    void* obj;
    const detail::meta_object* meta;
    object_ptr(void* obj, const detail::meta_object* meta) noexcept
      : obj(obj), meta(meta) {
      // nop
    }
    object_ptr(object_ptr&& other) noexcept : obj(nullptr), meta(nullptr) {
      using std::swap;
      swap(obj, other.obj);
      swap(meta, other.meta);
    }
    object_ptr& operator=(object_ptr&& other) noexcept {
      if (this != &other) {
        if (obj) {
          meta->destroy(obj);
          free(obj);
          obj = nullptr;
          meta = nullptr;
        }
        using std::swap;
        swap(obj, other.obj);
        swap(meta, other.meta);
      }
      return *this;
    }
    ~object_ptr() noexcept {
      if (obj) {
        meta->destroy(obj);
        free(obj);
      }
    }
  };
  struct free_t {
    void operator()(void* ptr) const noexcept {
      return free(ptr);
    }
  };
  using unique_void_ptr = std::unique_ptr<void, free_t>;
  auto msg_size = size_t{0};
  std::vector<object_ptr> objects;
  GUARDED(source.begin_sequence(msg_size));
  if (msg_size > 0) {
    // Deserialize message elements individually.
    detail::type_id_list_builder ids;
    objects.reserve(msg_size);
    auto data_size = size_t{0};
    for (size_t i = 0; i < msg_size; ++i) {
      auto type = type_id_t{0};
      GUARDED(source.fetch_next_object_type(type));
      if (auto meta_obj = detail::global_meta_object(type)) {
        ids.push_back(type);
        auto obj_size = meta_obj->padded_size;
        data_size += obj_size;
        unique_void_ptr vptr{malloc(obj_size)};
        if (vptr == nullptr)
          STOP(sec::runtime_error, "unable to allocate memory");
        meta_obj->default_construct(vptr.get());
        objects.emplace_back(vptr.release(), meta_obj);
        if (!load(*meta_obj, source, objects.back().obj))
          return false;
      } else {
        STOP(sec::unknown_type);
      }
    }
    GUARDED(source.end_sequence());
    // Merge elements into a single message data object.
    intrusive_ptr<detail::message_data> ptr;
    if (auto vptr = malloc(sizeof(detail::message_data) + data_size)) {
      // We don't need to worry about exceptions here: the message_data
      // constructor as well as `move_to_list` are `noexcept`.
      ptr.reset(new (vptr) detail::message_data(ids.move_to_list()), false);
    } else {
      STOP(sec::runtime_error, "unable to allocate memory");
    }
    auto pos = ptr->storage();
    for (auto& x : objects) {
      // TODO: avoid extra copy by adding move_construct to meta objects
      x.meta->copy_construct(pos, x.obj);
      ptr->inc_constructed_elements();
      pos += x.meta->padded_size;
    }
    data.reset(ptr.release(), false);
    return true;
  } else {
    data.reset();
    return source.end_sequence();
  }
}

} // namespace

bool message::load(deserializer& source) {
  return load_data(source, data_);
}

bool message::load(binary_deserializer& source) {
  return load_data(source, data_);
}

namespace {

bool save(const detail::meta_object& meta, caf::serializer& sink,
          const void* obj) {
  return meta.save(sink, obj);
}

bool save(const detail::meta_object& meta, caf::binary_serializer& sink,
          const void* obj) {
  return meta.save_binary(sink, obj);
}

template <class Serializer>
bool save_data(Serializer& sink, const message::data_ptr& data) {
  auto gmos = detail::global_meta_objects();
  // For machine-to-machine data formats, we prefix the type information.
  if (!sink.has_human_readable_format()) {
    if (data == nullptr) {
      // Short-circuit empty tuples.
      return sink.begin_object(type_id_v<message>, "message") //
             && sink.begin_field("types")                     //
             && sink.begin_sequence(0)                        //
             && sink.end_sequence()                           //
             && sink.end_field()                              //
             && sink.begin_field("values")                    //
             && sink.begin_tuple(0)                           //
             && sink.end_tuple()                              //
             && sink.end_field()                              //
             && sink.end_object();
    }
    GUARDED(sink.begin_object(type_id_v<message>, "message"));
    auto type_ids = data->types();
    // Write type information.
    GUARDED(sink.begin_field("types") && sink.begin_sequence(type_ids.size()));
    for (auto id : type_ids)
      GUARDED(sink.value(id));
    GUARDED(sink.end_sequence() && sink.end_field());
    // Write elements.
    auto storage = data->storage();
    GUARDED(sink.begin_field("values") && sink.begin_tuple(type_ids.size()));
    for (auto id : type_ids) {
      auto& meta = gmos[id];
      GUARDED(save(meta, sink, storage));
      storage += meta.padded_size;
    }
    return sink.end_tuple() && sink.end_field() && sink.end_object();
  }
  // For human-readable data formats, we serialize messages as a single list of
  // dynamically-typed objects.
  if (data == nullptr) {
    // Short-circuit empty tuples.
    return sink.begin_sequence(0) && sink.end_sequence();
  }
  auto type_ids = data->types();
  GUARDED(sink.begin_sequence(type_ids.size()));
  auto storage = data->storage();
  for (auto id : type_ids) {
    auto& meta = gmos[id];
    GUARDED(save(meta, sink, storage));
    storage += meta.padded_size;
  }
  return sink.end_sequence();
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
    return "message()";
  std::string result;
  result += "message(";
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
