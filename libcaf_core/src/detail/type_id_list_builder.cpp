/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/type_id_list_builder.hpp"

#include "caf/config.hpp"
#include "caf/type_id_list.hpp"

namespace caf::detail {

type_id_list_builder::type_id_list_builder()
  : size_(0), reserved_(0), storage_(nullptr) {
  // nop
}

type_id_list_builder::~type_id_list_builder() {
  free(storage_);
}

void type_id_list_builder::reserve(size_t new_capacity) {
  if (reserved_ >= new_capacity)
    return;
  reserved_ = new_capacity;
  auto ptr = realloc(storage_, reserved_ * sizeof(type_id_t));
  if (ptr == nullptr)
    throw std::bad_alloc();
}

void type_id_list_builder::push_back(type_id_t id) {
  if ((size_ + 1) >= reserved_) {
    reserved_ += block_size;
    auto ptr = realloc(storage_, reserved_ * sizeof(type_id_t));
    if (ptr == nullptr)
      throw std::bad_alloc();
    storage_ = reinterpret_cast<type_id_t*>(ptr);
    // Add the dummy for later inserting the size on first push_back.
    if (size_ == 0)
      storage_[0] = 0;
  }
  storage_[++size_] = id;
}

size_t type_id_list_builder::size() const noexcept {
  // Index 0 is reserved for storing the (final) size, i.e., does not contain a
  // type ID.
  return size_ > 0 ? size_ - 1 : 0;
}

type_id_t type_id_list_builder::operator[](size_t index) const noexcept{
  CAF_ASSERT(index<size());
  return storage_[index + 1];
}

type_id_list type_id_list_builder::to_list() noexcept {
  // TODO: implement me!
  return make_type_id_list();
}

// type_id_list type_id_list_builder::copy_to_list() {
//   storage_[0]
//     = static_cast<type_id_t>(size_ & type_id_list::dynamically_allocated_flag);
//   auto ptr = malloc((size_ + 1) * sizeof(type_id_t));
//   if (ptr == nullptr)
//     throw std::bad_alloc(
//       "type_id_list_builder::copy_to_list failed to allocate memory");
//   memcpy(ptr, storage_, (size_ + 1) * sizeof(type_id_t));
//   return type_id_list{reinterpret_cast<type_id_t*>(ptr)};
// }

} // namespace caf::detail
