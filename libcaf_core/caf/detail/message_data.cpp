// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/message_data.hpp"

#include "caf/detail/assert.hpp"
#include "caf/detail/meta_object.hpp"
#include "caf/error.hpp"
#include "caf/error_code.hpp"
#include "caf/message.hpp"
#include "caf/raise_error.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

#include <cstring>
#include <numeric>

namespace caf::detail {

message_data::message_data(type_id_list types) noexcept
  : rc_(1), types_(std::move(types)), constructed_elements_(0) {
  // nop
}

message_data::~message_data() noexcept {
  // Note: no need to perform bound checks or nullptr checks here, because
  //       we verify the type IDs while constructing the message.
  auto gmos = global_meta_objects();
  auto ptr = storage();
  if (constructed_elements_ == types_.size()) {
    for (auto id : types_) {
      auto& meta = gmos[id];
      meta.destroy(ptr);
      ptr += meta.padded_size;
    }
  } else {
    for (size_t index = 0; index < constructed_elements_; ++index) {
      auto& meta = gmos[types_[index]];
      meta.destroy(ptr);
      ptr += meta.padded_size;
    }
  }
}

message_data* message_data::copy() const {
  // Note: no need to perform bound checks or nullptr checks here, because
  //       we verify the type IDs while constructing the original message.
  auto gmos = global_meta_objects();
  size_t storage_size = 0;
  for (auto id : types_)
    storage_size += gmos[id].padded_size;
  auto total_size = sizeof(message_data) + storage_size;
  auto vptr = malloc(total_size);
  if (vptr == nullptr)
    CAF_RAISE_ERROR(std::bad_alloc, "bad_alloc");
  intrusive_ptr<message_data> ptr{new (vptr) message_data(types_), false};
  auto src = storage();
  auto dst = ptr->storage();
  for (auto id : types_) {
    auto& meta = gmos[id];
    meta.copy_construct(dst, src);
    ++ptr->constructed_elements_;
    src += meta.padded_size;
    dst += meta.padded_size;
  }
  return ptr.release();
}

intrusive_ptr<message_data>
message_data::make_uninitialized(type_id_list types) {
  size_t storage_size = 0;
  for (auto id : types)
    storage_size += global_meta_object(id).padded_size;
  auto total_size = sizeof(message_data) + storage_size;
  auto vptr = malloc(total_size);
  if (vptr == nullptr)
    CAF_RAISE_ERROR(std::bad_alloc, "bad_alloc");
  return {new (vptr) message_data(types), false};
}

std::byte* message_data::at(size_t index) noexcept {
  if (index == 0)
    return storage();
  auto gmos = global_meta_objects();
  auto ptr = storage();
  for (size_t i = 0; i < index; ++i)
    ptr += gmos[types_[i]].padded_size;
  return ptr;
}

const std::byte* message_data::at(size_t index) const noexcept {
  if (index == 0)
    return storage();
  auto gmos = global_meta_objects();
  auto ptr = storage();
  for (size_t i = 0; i < index; ++i)
    ptr += gmos[types_[i]].padded_size;
  return ptr;
}

std::byte* message_data::stepwise_init_from(std::byte* pos,
                                            const message& msg) {
  return stepwise_init_from(pos, msg.cptr());
}

std::byte* message_data::stepwise_init_from(std::byte* pos,
                                            const message_data* other) {
  CAF_ASSERT(other != nullptr);
  CAF_ASSERT(other != this);
  auto gmos = global_meta_objects();
  auto src = other->storage();
  for (auto id : other->types()) {
    auto& meta = gmos[id];
    meta.copy_construct(pos, src);
    ++constructed_elements_;
    src += meta.padded_size;
    pos += meta.padded_size;
  }
  return pos;
}

} // namespace caf::detail
