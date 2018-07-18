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

#include "caf/type_erased_tuple.hpp"

#include "caf/config.hpp"
#include "caf/error.hpp"
#include "caf/raise_error.hpp"

#include "caf/detail/try_match.hpp"

namespace caf {

type_erased_tuple::~type_erased_tuple() {
  // nop
}

error type_erased_tuple::load(deserializer& source) {
  for (size_t i = 0; i < size(); ++i) {
    auto e = load(i, source);
    if (e)
      return e;
  }
  return none;
}

bool type_erased_tuple::shared() const noexcept {
  return false;
}

bool type_erased_tuple::empty() const {
  return size() == 0;
}

std::string type_erased_tuple::stringify() const {
  if (size() == 0)
    return "()";
  std::string result = "(";
  result += stringify(0);
  for (size_t i = 1; i < size(); ++i) {
    result += ", ";
    result += stringify(i);
  }
  result += ')';
  return result;
}

error type_erased_tuple::save(serializer& sink) const {
  for (size_t i = 0; i < size(); ++i) {
    auto e = save(i, sink);
    if (e)
      return e;
  }
  return none;
}

bool type_erased_tuple::matches(size_t pos, uint16_t nr,
                                const std::type_info* ptr) const noexcept {
  CAF_ASSERT(pos < size());
  auto tp = type(pos);
  if (tp.first != nr)
    return false;
  if (nr == 0)
    return ptr != nullptr ? *tp.second == *ptr : false;
  return true;
}

empty_type_erased_tuple::~empty_type_erased_tuple() {
  // nop
}

void* empty_type_erased_tuple::get_mutable(size_t) {
  CAF_RAISE_ERROR("empty_type_erased_tuple::get_mutable");
}

error empty_type_erased_tuple::load(size_t, deserializer&) {
  CAF_RAISE_ERROR("empty_type_erased_tuple::get_mutable");
}

size_t empty_type_erased_tuple::size() const noexcept {
  return 0;
}

uint32_t empty_type_erased_tuple::type_token() const noexcept {
  return make_type_token();
}

auto empty_type_erased_tuple::type(size_t) const noexcept -> rtti_pair {
  CAF_CRITICAL("empty_type_erased_tuple::type");
}

const void* empty_type_erased_tuple::get(size_t) const noexcept {
  CAF_CRITICAL("empty_type_erased_tuple::get");
}

std::string empty_type_erased_tuple::stringify(size_t) const {
  CAF_RAISE_ERROR("empty_type_erased_tuple::stringify");
}

type_erased_value_ptr empty_type_erased_tuple::copy(size_t) const {
  CAF_RAISE_ERROR("empty_type_erased_tuple::copy");
}

error empty_type_erased_tuple::save(size_t, serializer&) const {
  CAF_RAISE_ERROR("empty_type_erased_tuple::copy");
}

} // namespace caf
