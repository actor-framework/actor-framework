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

#include "caf/detail/dynamic_message_data.hpp"

#include "caf/make_counted.hpp"

namespace caf {
namespace detail {

dynamic_message_data::dynamic_message_data() : type_token_(0xFFFFFFFF) {
  // nop
}

dynamic_message_data::dynamic_message_data(const dynamic_message_data& other)
    : detail::message_data(other),
      type_token_(0xFFFFFFFF) {
  for (auto& e : other.elements_) {
    add_to_type_token(e->type().first);
    elements_.emplace_back(e->copy());
  }
}

dynamic_message_data::dynamic_message_data(elements&& data)
    : elements_(std::move(data)),
      type_token_(0xFFFFFFFF) {
  for (auto& e : elements_)
    add_to_type_token(e->type().first);
}

dynamic_message_data::~dynamic_message_data() {
  // nop
}

const void* dynamic_message_data::at(size_t pos) const {
  CAF_ASSERT(pos < size());
  return elements_[pos]->get();
}

void dynamic_message_data::serialize_at(serializer& sink, size_t pos) const {
  elements_[pos]->save(sink);
}

void dynamic_message_data::serialize_at(deserializer& source, size_t pos) {
  elements_[pos]->load(source);
}

bool dynamic_message_data::compare_at(size_t pos, const element_rtti& rtti,
                                      const void* x) const {
  return match_element(pos, rtti.first, rtti.second)
         ? elements_[pos]->equals(x)
         : false;
}

void* dynamic_message_data::mutable_at(size_t pos) {
  CAF_ASSERT(pos < size());
  return elements_[pos]->get();
}

size_t dynamic_message_data::size() const {
  return elements_.size();
}

message_data::cow_ptr dynamic_message_data::copy() const {
  return make_counted<dynamic_message_data>(*this);
}

bool dynamic_message_data::match_element(size_t pos, uint16_t nr,
                                         const std::type_info* ptr) const {
  CAF_ASSERT(nr != 0 || ptr != nullptr);
  auto rtti = type_at(pos);
  return rtti.first == nr && (nr != 0 || *rtti.second == *ptr);
}

auto dynamic_message_data::type_at(size_t pos) const -> element_rtti {
  return elements_[pos]->type();
}

std::string dynamic_message_data::stringify_at(size_t pos) const {
  return elements_[pos]->stringify();
}

uint32_t dynamic_message_data::type_token() const {
  return type_token_;
}

void dynamic_message_data::append(type_erased_value_ptr x) {
  add_to_type_token(x->type().first);
  elements_.emplace_back(std::move(x));
}

void dynamic_message_data::add_to_type_token(uint16_t typenr) {
  type_token_ = (type_token_ << 6) | typenr;
}

void dynamic_message_data::clear() {
  elements_.clear();
  type_token_ = 0xFFFFFFFF;
}

} // namespace detail
} // namespace caf
