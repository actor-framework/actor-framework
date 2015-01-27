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

#ifndef CAF_DETAIL_MESSAGE_ITERATOR_HPP
#define CAF_DETAIL_MESSAGE_ITERATOR_HPP

#include <cstddef>
#include <typeinfo>

#include "caf/fwd.hpp"

namespace caf {
namespace detail {

class message_data;

class message_iterator {
 public:
  using pointer = message_data*;
  using const_pointer = const message_data*;

  message_iterator(const_pointer data, size_t pos = 0);

  message_iterator(const message_iterator&) = default;

  message_iterator& operator=(const message_iterator&) = default;

  inline message_iterator& operator++() {
    ++m_pos;
    return *this;
  }

  inline message_iterator& operator--() {
    --m_pos;
    return *this;
  }

  inline message_iterator operator+(size_t offset) {
    return {m_data, m_pos + offset};
  }

  inline message_iterator& operator+=(size_t offset) {
    m_pos += offset;
    return *this;
  }

  inline message_iterator operator-(size_t offset) {
    return {m_data, m_pos - offset};
  }

  inline message_iterator& operator-=(size_t offset) {
    m_pos -= offset;
    return *this;
  }

  inline size_t position() const {
    return m_pos;
  }

  inline const_pointer data() const {
    return m_data;
  }

  const void* value() const;

  bool match_element(uint16_t typenr, const std::type_info* rtti) const;

  template <class T>
  const T& value_as() const {
    return *reinterpret_cast<const T*>(value());
  }

  inline message_iterator& operator*() {
    return *this;
  }

 private:
  size_t m_pos;
  const message_data* m_data;
};

inline bool operator==(const message_iterator& lhs,
                       const message_iterator& rhs) {
  return lhs.data() == rhs.data() && lhs.position() == rhs.position();
}

inline bool operator!=(const message_iterator& lhs,
                       const message_iterator& rhs) {
  return !(lhs == rhs);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MESSAGE_ITERATOR_HPP
