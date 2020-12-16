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

#include "caf/detail/message_data.hpp"

#include <cstring>
#include <numeric>

#include "caf/detail/meta_object.hpp"
#include "caf/error.hpp"
#include "caf/error_code.hpp"
#include "caf/raise_error.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

namespace caf::detail {

message_data::message_data(type_id_list types) noexcept
  : rc_(1), types_(std::move(types)), constructed_elements_(0) {
  // nop
}

message_data::~message_data() noexcept {
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

byte* message_data::at(size_t index) noexcept {
  if (index == 0)
    return storage();
  auto gmos = global_meta_objects();
  auto ptr = storage();
  for (size_t i = 0; i < index; ++i)
    ptr += gmos[types_[i]].padded_size;
  return ptr;
}

const byte* message_data::at(size_t index) const noexcept {
  if (index == 0)
    return storage();
  auto gmos = global_meta_objects();
  auto ptr = storage();
  for (size_t i = 0; i < index; ++i)
    ptr += gmos[types_[i]].padded_size;
  return ptr;
}

} // namespace caf::detail
