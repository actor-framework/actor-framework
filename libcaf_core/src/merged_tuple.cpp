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

#include "caf/detail/merged_tuple.hpp"

#include "caf/index_mapping.hpp"
#include "caf/system_messages.hpp"

#include "caf/detail/disposer.hpp"

namespace caf {
namespace detail {

merged_tuple::merged_tuple(data_type xs, mapping_type ys)
    : data_(std::move(xs)),
      type_token_(0xFFFFFFFF),
      mapping_(std::move(ys)) {
  CAF_ASSERT(! data_.empty());
  CAF_ASSERT(! mapping_.empty());
  // calculate type token
  for (size_t i = 0; i < mapping_.size(); ++i) {
    type_token_ <<= 6;
    auto& p = mapping_[i];
    type_token_ |= data_[p.first]->type_nr_at(p.second);
  }
}

// creates a typed subtuple from `d` with mapping `v`
merged_tuple::cow_ptr merged_tuple::make(message x, message y) {
  data_type data{x.vals(), y.vals()};
  mapping_type mapping;
  auto s = x.size();
  for (size_t i = 0; i < s; ++i) {
    if (x.match_element<index_mapping>(i))
      mapping.emplace_back(1, x.get_as<index_mapping>(i).value - 1);
    else
      mapping.emplace_back(0, i);
  }
  return cow_ptr{make_counted<merged_tuple>(std::move(data),
                                            std::move(mapping))};
}

void* merged_tuple::mutable_at(size_t pos) {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  return data_[p.first]->mutable_at(p.second);
}

void merged_tuple::serialize_at(deserializer& source, size_t pos) {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  data_[p.first]->serialize_at(source, p.second);
}

size_t merged_tuple::size() const {
  return mapping_.size();
}

merged_tuple::cow_ptr merged_tuple::copy() const {
  return cow_ptr{make_counted<merged_tuple>(data_, mapping_)};
}

const void* merged_tuple::at(size_t pos) const {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  return data_[p.first]->at(p.second);
}

bool merged_tuple::compare_at(size_t pos, const element_rtti& rtti,
                              const void* x) const {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  return data_[p.first]->compare_at(p.second, rtti, x);
}

bool merged_tuple::match_element(size_t pos, uint16_t typenr,
                                 const std::type_info* rtti) const {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  return data_[p.first]->match_element(p.second, typenr, rtti);
}

uint32_t merged_tuple::type_token() const {
  return type_token_;
}

merged_tuple::element_rtti merged_tuple::type_at(size_t pos) const {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  return data_[p.first]->type_at(p.second);
}

void merged_tuple::serialize_at(serializer& sink, size_t pos) const {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  data_[p.first]->serialize_at(sink, p.second);
}

std::string merged_tuple::stringify_at(size_t pos) const {
  CAF_ASSERT(pos < mapping_.size());
  auto& p = mapping_[pos];
  return data_[p.first]->stringify_at(p.second);
}

const merged_tuple::mapping_type& merged_tuple::mapping() const {
  return mapping_;
}

} // namespace detail
} // namespace caf
