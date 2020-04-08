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

#include "caf/message_builder.hpp"

namespace caf {

void message_builder::clear() noexcept {
  storage_size_ = 0;
  types_.clear();
  elements_.clear();
}

message message_builder::to_message() const {
  if (empty())
    return message{};
  using namespace detail;
  auto vptr = malloc(sizeof(message_data) + storage_size_);
  if (vptr == nullptr)
    throw std::bad_alloc();
  auto raw_ptr = new (vptr) message_data(types_.copy_to_list());
  intrusive_cow_ptr<message_data> ptr{raw_ptr, false};
  auto storage = raw_ptr->storage();
  for (auto& element : elements_)
    storage = element->copy_init(storage);
  return message{std::move(ptr)};
}

message message_builder::move_to_message() {
  if (empty())
    return message{};
  using namespace detail;
  auto vptr = malloc(sizeof(message_data) + storage_size_);
  if (vptr == nullptr)
    throw std::bad_alloc();
  auto raw_ptr = new (vptr) message_data(types_.move_to_list());
  intrusive_cow_ptr<message_data> ptr{raw_ptr, false};
  auto storage = raw_ptr->storage();
  for (auto& element : elements_)
    storage = element->move_init(storage);
  return message{std::move(ptr)};
}

} // namespace caf
