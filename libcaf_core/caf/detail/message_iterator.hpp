/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_DETAIL_TUPLE_ITERATOR_HPP
#define CAF_DETAIL_TUPLE_ITERATOR_HPP

#include <cstddef>

#include "caf/config.hpp"

namespace caf {
namespace detail {

template <class Tuple>
class message_iterator {

  size_t m_pos;
  const Tuple* m_tuple;

 public:

  inline message_iterator(const Tuple* tup, size_t pos = 0)
      : m_pos(pos), m_tuple(tup) {}

  message_iterator(const message_iterator&) = default;

  message_iterator& operator=(const message_iterator&) = default;

  inline bool operator==(const message_iterator& other) const {
    CAF_REQUIRE(other.m_tuple == other.m_tuple);
    return other.m_pos == m_pos;
  }

  inline bool operator!=(const message_iterator& other) const {
    return !(*this == other);
  }

  inline message_iterator& operator++() {
    ++m_pos;
    return *this;
  }

  inline message_iterator& operator--() {
    CAF_REQUIRE(m_pos > 0);
    --m_pos;
    return *this;
  }

  inline message_iterator operator+(size_t offset) {
    return {m_tuple, m_pos + offset};
  }

  inline message_iterator& operator+=(size_t offset) {
    m_pos += offset;
    return *this;
  }

  inline message_iterator operator-(size_t offset) {
    CAF_REQUIRE(m_pos >= offset);
    return {m_tuple, m_pos - offset};
  }

  inline message_iterator& operator-=(size_t offset) {
    CAF_REQUIRE(m_pos >= offset);
    m_pos -= offset;
    return *this;
  }

  inline size_t position() const { return m_pos; }

  inline const void* value() const { return m_tuple->at(m_pos); }

  inline const uniform_type_info* type() const {
    return m_tuple->type_at(m_pos);
  }

  inline message_iterator& operator*() { return *this; }

};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TUPLE_ITERATOR_HPP
