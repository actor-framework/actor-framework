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

#include "caf/detail/dynamic_message_data.hpp"

#include "caf/error.hpp"
#include "caf/intrusive_cow_ptr.hpp"
#include "caf/make_counted.hpp"

namespace caf {
namespace detail {

dynamic_message_data::dynamic_message_data() : type_token_(0xFFFFFFFF) {
  // nop
}

dynamic_message_data::dynamic_message_data(elements&& data)
    : elements_(std::move(data)),
      type_token_(0xFFFFFFFF) {
  for (auto& e : elements_)
    add_to_type_token(e->type().first);
}

dynamic_message_data::dynamic_message_data(const dynamic_message_data& other)
    : detail::message_data(other),
      type_token_(0xFFFFFFFF) {
  for (auto& e : other.elements_) {
    add_to_type_token(e->type().first);
    elements_.emplace_back(e->copy());
  }
}

dynamic_message_data::~dynamic_message_data() {
  // nop
}

dynamic_message_data* dynamic_message_data::copy() const {
  return new dynamic_message_data(*this);
}

void* dynamic_message_data::get_mutable(size_t pos) {
  CAF_ASSERT(pos < size());
  return elements_[pos]->get_mutable();
}

error dynamic_message_data::load(size_t pos, deserializer& source) {
  CAF_ASSERT(pos < size());
  return elements_[pos]->load(source);
}

size_t dynamic_message_data::size() const noexcept {
  return elements_.size();
}

uint32_t dynamic_message_data::type_token() const noexcept {
  return type_token_;
}

auto dynamic_message_data::type(size_t pos) const noexcept -> rtti_pair {
  CAF_ASSERT(pos < size());
  return elements_[pos]->type();
}

const void* dynamic_message_data::get(size_t pos) const noexcept {
  CAF_ASSERT(pos < size());
  return elements_[pos]->get();
}

std::string dynamic_message_data::stringify(size_t pos) const {
  CAF_ASSERT(pos < size());
  return elements_[pos]->stringify();
}

type_erased_value_ptr dynamic_message_data::copy(size_t pos) const {
  CAF_ASSERT(pos < size());
  return elements_[pos]->copy();
}

error dynamic_message_data::save(size_t pos, serializer& sink) const {
  CAF_ASSERT(pos < size());
  return elements_[pos]->save(sink);
}

void dynamic_message_data::clear() {
  elements_.clear();
  type_token_ = 0xFFFFFFFF;
}

void dynamic_message_data::append(type_erased_value_ptr x) {
  add_to_type_token(x->type().first);
  elements_.emplace_back(std::move(x));
}

void dynamic_message_data::add_to_type_token(uint16_t typenr) {
  type_token_ = (type_token_ << 6) | typenr;
}

void intrusive_ptr_add_ref(const dynamic_message_data* ptr) {
  intrusive_ptr_add_ref(static_cast<const ref_counted*>(ptr));
}

void intrusive_ptr_release(const dynamic_message_data* ptr) {
  intrusive_ptr_release(static_cast<const ref_counted*>(ptr));
}

dynamic_message_data* intrusive_cow_ptr_unshare(dynamic_message_data*& ptr) {
  return default_intrusive_cow_ptr_unshare(ptr);
}

} // namespace detail
} // namespace caf
