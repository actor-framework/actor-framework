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

#include "caf/detail/concatenated_tuple.hpp"

#include <numeric>

#include "caf/make_counted.hpp"
#include "caf/message.hpp"
#include "caf/raise_error.hpp"

namespace caf {
namespace detail {

concatenated_tuple::concatenated_tuple(std::initializer_list<cow_ptr> xs) {
  for (auto& x : xs) {
    if (x) {
      auto dptr = dynamic_cast<const concatenated_tuple*>(x.get());
      if (dptr != nullptr) {
        auto& vec = dptr->data_;
        data_.insert(data_.end(), vec.begin(), vec.end());
      } else {
        data_.push_back(std::move(x));
      }
    }
  }
  type_token_ = make_type_token();
  for (const auto& m : data_)
    for (size_t i = 0; i < m->size(); ++i)
      type_token_ = add_to_type_token(type_token_, m->type_nr(i));
  auto acc_size = [](size_t tmp, const cow_ptr& val) {
    return tmp + val->size();
  };
  size_ = std::accumulate(data_.begin(), data_.end(), size_t{0}, acc_size);
}

auto concatenated_tuple::make(std::initializer_list<cow_ptr> xs) -> cow_ptr {
  return cow_ptr{make_counted<concatenated_tuple>(xs)};
}

concatenated_tuple* concatenated_tuple::copy() const {
  return new concatenated_tuple(*this);
}

void* concatenated_tuple::get_mutable(size_t pos) {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->get_mutable(selected.second);
}

error concatenated_tuple::load(size_t pos, deserializer& source) {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->load(selected.second, source);
}

size_t concatenated_tuple::size() const noexcept {
  return size_;
}

uint32_t concatenated_tuple::type_token() const noexcept {
  return type_token_;
}

rtti_pair concatenated_tuple::type(size_t pos) const noexcept {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->type(selected.second);
}

const void* concatenated_tuple::get(size_t pos) const noexcept {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->get(selected.second);
}

std::string concatenated_tuple::stringify(size_t pos) const {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->stringify(selected.second);
}

type_erased_value_ptr concatenated_tuple::copy(size_t pos) const {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->copy(selected.second);
}

error concatenated_tuple::save(size_t pos, serializer& sink) const {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->save(selected.second, sink);
}

std::pair<message_data*, size_t> concatenated_tuple::select(size_t pos) {
  auto idx = pos;
  for (auto& m : data_) {
    auto s = m->size();
    if (idx >= s)
      idx -= s;
    else
      return {m.unshared_ptr(), idx};
  }
  CAF_RAISE_ERROR(std::out_of_range, "concatenated_tuple::select out of range");
}

std::pair<const message_data*, size_t>
concatenated_tuple::select(size_t pos) const {
  auto idx = pos;
  for (const auto& m : data_) {
    auto s = m->size();
    if (idx >= s)
      idx -= s;
    else
      return {m.get(), idx};
  }
  CAF_RAISE_ERROR(std::out_of_range, "concatenated_tuple::select out of range");
}

} // namespace detail
} // namespace caf
