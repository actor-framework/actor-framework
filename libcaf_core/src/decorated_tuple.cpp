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

#include "caf/detail/decorated_tuple.hpp"

#include "caf/make_counted.hpp"

namespace caf {
namespace detail {

decorated_tuple::decorated_tuple(cow_ptr&& d, vector_type&& v)
    : decorated_(std::move(d)),
      mapping_(std::move(v)),
      type_token_(0xFFFFFFFF) {
  CAF_ASSERT(mapping_.empty()
              || *(std::max_element(mapping_.begin(), mapping_.end()))
                 < static_cast<const cow_ptr&>(decorated_)->size());
  // calculate type token
  for (size_t i = 0; i < mapping_.size(); ++i) {
    type_token_ <<= 6;
    type_token_ |= decorated_->type_nr(mapping_[i]);
  }
}

decorated_tuple::cow_ptr decorated_tuple::make(cow_ptr d, vector_type v) {
  auto ptr = dynamic_cast<const decorated_tuple*>(d.get());
  if (ptr) {
    d = ptr->decorated();
    auto& pmap = ptr->mapping();
    for (size_t i = 0; i < v.size(); ++i) {
      v[i] = pmap[v[i]];
    }
  }
  return make_counted<decorated_tuple>(std::move(d), std::move(v));
}

message_data::cow_ptr decorated_tuple::copy() const {
  return cow_ptr(new decorated_tuple(*this), false);
}

void* decorated_tuple::get_mutable(size_t pos) {
  CAF_ASSERT(pos < size());
  return decorated_->get_mutable(mapping_[pos]);
}

error decorated_tuple::load(size_t pos, deserializer& source) {
  CAF_ASSERT(pos < size());
  return decorated_->load(mapping_[pos], source);
}

size_t decorated_tuple::size() const noexcept {
  return mapping_.size();
}

uint32_t decorated_tuple::type_token() const noexcept {
  return type_token_;
}

message_data::rtti_pair decorated_tuple::type(size_t pos) const noexcept {
  CAF_ASSERT(pos < size());
  return decorated_->type(mapping_[pos]);
}

const void* decorated_tuple::get(size_t pos) const noexcept {
  CAF_ASSERT(pos < size());
  return decorated_->get(mapping_[pos]);
}

std::string decorated_tuple::stringify(size_t pos) const {
  CAF_ASSERT(pos < size());
  return decorated_->stringify(mapping_[pos]);
}

type_erased_value_ptr decorated_tuple::copy(size_t pos) const {
  CAF_ASSERT(pos < size());
  return decorated_->copy(mapping_[pos]);
}

error decorated_tuple::save(size_t pos, serializer& sink) const {
  CAF_ASSERT(pos < size());
  return decorated_->save(mapping_[pos], sink);
}

} // namespace detail
} // namespace caf
