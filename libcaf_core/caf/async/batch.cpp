// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/async/batch.hpp"

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/deserializer.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/serializer.hpp"

namespace caf::async {

// -- batch::data --------------------------------------------------------------

namespace {

bool do_save(const detail::meta_object& meta, serializer& sink,
             const void* ptr) {
  return meta.save(sink, ptr);
}

bool do_save(const detail::meta_object& meta, binary_serializer& sink,
             const void* ptr) {
  return meta.save_binary(sink, ptr);
}

bool do_load(const detail::meta_object& meta, deserializer& sink, void* ptr) {
  return meta.load(sink, ptr);
}

bool do_load(const detail::meta_object& meta, binary_deserializer& sink,
             void* ptr) {
  return meta.load_binary(sink, ptr);
}

void dynamic_item_destructor(type_id_t item_type, size_t item_size,
                             size_t array_size, std::byte* data_ptr) {
  CAF_ASSERT(item_size > 0);
  CAF_ASSERT(array_size > 0);
  auto& meta = detail::global_meta_object(item_type);
  do {
    meta.destroy(data_ptr);
    data_ptr += item_size;
    --array_size;
  } while (array_size > 0);
}

} // namespace

template <class Inspector>
bool batch::data::save(Inspector& sink) const {
  CAF_ASSERT(size_ > 0);
  const auto* meta = detail::global_meta_object_or_null(item_type_);
  if (!meta) {
    sink.emplace_error(sec::unsafe_type);
    return false;
  }
  if (!sink.begin_object(type_id_v<batch>, type_name_v<batch>))
    return false;
  // The "type" field adds run-time type information to the batch. We use the
  // type name instead of the type ID for human-readable output.
  if (!sink.begin_field("type", true))
    return false;
  if (!sink.has_human_readable_format()) {
    if (!sink.value(item_type_))
      return false;
  } else {
    if (!sink.value(meta->type_name))
      return false;
  }
  if (!sink.end_field())
    return false;
  // The "items" field contains the actual batch data.
  if (!sink.begin_field("items", true))
    return false;
  auto ptr = storage_;
  if (!sink.begin_sequence(size_))
    return false;
  auto len = size_;
  do {
    if (!do_save(*meta, sink, ptr))
      return false;
    ptr += item_size_;
    --len;
  } while (len > 0);
  return sink.end_sequence() && sink.end_field() && sink.end_object();
}

// -- batch --------------------------------------------------------------------

template <class Inspector>
bool batch::save_impl(Inspector& sink) const {
  if (data_)
    return data_->save(sink);
  return sink.begin_object(type_id_v<batch>, type_name_v<batch>) //
         && sink.begin_field("type", false)                      //
         && sink.end_field()                                     //
         && sink.begin_field("items", false)                     //
         && sink.end_field()                                     //
         && sink.end_object();
}

bool batch::save(serializer& f) const {
  return save_impl(f);
}

bool batch::save(binary_serializer& f) const {
  return save_impl(f);
}
template <class Inspector>
bool batch::load_impl(Inspector& source) {
  if (!source.begin_object(type_id_v<batch>, type_name_v<batch>))
    return false;
  bool type_field_present = false;
  // The "type" field adds run-time type information to the batch. We use the
  // type name instead of the type ID for human-readable output.
  if (!source.begin_field("type", type_field_present))
    return false;
  if (!type_field_present) {
    // Only an empty batch may omit the "type" field. Hence, the "items" field
    // must also be omitted.
    if (!source.end_field())
      return false;
    auto items_field_present = false;
    if (!source.begin_field("items", items_field_present))
      return false;
    if (items_field_present) {
      source.emplace_error(sec::field_invariant_check_failed,
                           "a batch without a type may not contain items");
      return false;
    }
    return source.end_field() && source.end_object();
  }
  auto item_type = invalid_type_id;
  if (!source.has_human_readable_format()) {
    if (!source.value(item_type))
      return false;
  } else {
    std::string type_name;
    if (!source.value(type_name))
      return false;
    item_type = query_type_id(type_name);
  }
  const auto* meta = detail::global_meta_object_or_null(item_type);
  if (!meta) {
    source.emplace_error(sec::unknown_type);
    return false;
  }
  if (!source.end_field())
    return false;
  // The "items" field contains the actual batch data.
  auto items_field_present = false;
  if (!source.begin_field("items", items_field_present))
    return false;
  if (!items_field_present) {
    data_.reset();
    return source.end_field() && source.end_object();
  }
  auto len = size_t{0};
  if (!source.begin_sequence(len))
    return false;
  if (len == 0) {
    data_.reset();
    return source.end_sequence() && source.end_field() && source.end_object();
  }
  // Allocate storage for the batch.
  auto total_size = sizeof(batch::data) + (len * meta->simple_size);
  auto vptr = malloc(total_size);
  if (vptr == nullptr) {
    source.emplace_error(sec::load_callback_failed, "malloc failed");
    return false;
  }
  // We start the item count at 0 and increment it for each successfully loaded
  // item. This makes sure that the destructor only destroys fully constructed
  // items in case of an error or exception.
  intrusive_ptr<batch::data> ptr{new (vptr)
                                   batch::data(dynamic_item_destructor,
                                               item_type, meta->simple_size, 0),
                                 false};
  auto* storage = ptr->storage_;
  for (auto i = size_t{0}; i < len; ++i) {
    meta->default_construct(storage);
    if (!do_load(*meta, source, storage))
      return false;
    ++ptr->size_;
    storage += meta->simple_size;
  }
  data_ = std::move(ptr);
  return source.end_sequence() && source.end_field() && source.end_object();
}

bool batch::load(deserializer& f) {
  return load_impl(f);
}

bool batch::load(binary_deserializer& f) {
  return load_impl(f);
}

} // namespace caf::async
