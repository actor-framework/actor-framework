/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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
#include "caf/make_counted.hpp"

#include "caf/detail/concatenated_tuple.hpp"

#include <numeric>

namespace caf {
namespace detail {

auto concatenated_tuple::make(std::initializer_list<cow_ptr> xs) -> cow_ptr {
  return cow_ptr{make_counted<concatenated_tuple>(xs)};
}

void* concatenated_tuple::mutable_at(size_t pos) {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->mutable_at(selected.second);
}

size_t concatenated_tuple::size() const {
  return size_;
}

message_data::cow_ptr concatenated_tuple::copy() const {
  return cow_ptr(new concatenated_tuple(*this), false);
}

const void* concatenated_tuple::at(size_t pos) const {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->at(selected.second);
}

bool concatenated_tuple::match_element(size_t pos, uint16_t typenr,
                                       const std::type_info* rtti) const {
  auto selected = select(pos);
  return selected.first->match_element(selected.second, typenr, rtti);
}

uint32_t concatenated_tuple::type_token() const {
  return type_token_;
}

const char* concatenated_tuple::uniform_name_at(size_t pos) const {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->uniform_name_at(selected.second);
}

uint16_t concatenated_tuple::type_nr_at(size_t pos) const {
  CAF_ASSERT(pos < size());
  auto selected = select(pos);
  return selected.first->type_nr_at(selected.second);
}

std::pair<message_data*, size_t> concatenated_tuple::select(size_t pos) const {
  auto idx = pos;
  for (auto& m : data_) {
    auto s = m->size();
    if (idx >= s) {
      idx -= s;
    } else {
      return {m.get(), idx};
    }
  }
  throw std::out_of_range("out of range: concatenated_tuple::select");
}

concatenated_tuple::concatenated_tuple(std::initializer_list<cow_ptr> xs) {
  for (auto& x : xs) {
    if (x) {
      auto dptr = dynamic_cast<const concatenated_tuple*>(x.get());
      if (dptr) {
        auto& vec = dptr->data_;
        data_.insert(data_.end(), vec.begin(), vec.end());
      } else {
        data_.push_back(std::move(x));
      }
    }
  }
  type_token_ = make_type_token();
  for (auto& m : data_) {
    for (size_t i = 0; i < m->size(); ++i) {
      type_token_ = add_to_type_token(type_token_, m->type_nr_at(i));
    }
  }
  auto acc_size = [](size_t tmp, const cow_ptr& val) {
    return tmp + val->size();
  };
  size_ = std::accumulate(data_.begin(), data_.end(), size_t{0}, acc_size);
}

} // namespace detail
} // namespace caf
