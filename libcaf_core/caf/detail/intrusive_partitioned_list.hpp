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

#ifndef CAF_DETAIL_INTRUSIVE_LIST_HPP
#define CAF_DETAIL_INTRUSIVE_LIST_HPP

#include <memory>
#include <iterator>

namespace caf {
namespace detail {

template <class T, class Delete = std::default_delete<T>>
class intrusive_partitioned_list {
 public:
  using value_type = T;
  using pointer = value_type*;
  using deleter_type = Delete;

  struct iterator : std::iterator<std::bidirectional_iterator_tag, value_type> {
    pointer ptr;

    iterator(pointer init = nullptr) : ptr(init) {
      // nop
    }

    iterator(const iterator&) = default;
    iterator& operator=(const iterator&) = default;

    iterator& operator++() {
      ptr = ptr->next;
      return *this;
    }

    iterator operator++(int) {
      iterator res = *this;
      ptr = ptr->next;
      return res;
    }

    iterator& operator--() {
      ptr = ptr->prev;
      return *this;
    }

    iterator operator--(int) {
      iterator res = *this;
      ptr = ptr->prev;
      return res;
    }

    value_type& operator*() {
      return *ptr;
    }

    pointer operator->() {
      return ptr;
    }

    bool operator==(const iterator& other) const {
      return ptr == other.ptr;
    }

    bool operator!=(const iterator& other) const {
      return ptr != other.ptr;
    }
  };

  intrusive_partitioned_list() {
    m_head.next = &m_separator;
    m_separator.prev = &m_head;
    m_separator.next = &m_tail;
    m_tail.prev = &m_separator;
  }

  iterator first_begin() {
    return m_head.next;
  }

  iterator first_end() {
    return &m_separator;
  }

  iterator second_begin() {
    return m_separator.next;
  }

  iterator second_end() {
    return &m_tail;
  }

  iterator insert(iterator next, pointer val) {
    auto prev = next->prev;
    val->prev = prev;
    val->next = next.ptr;
    prev->next = val;
    next->prev = val;
    return val;
  }

  bool first_empty() {
    return first_begin() == first_end();
  }

  bool second_empty() {
    return second_begin() == second_end();
  }

  bool empty() {
    return first_empty() && second_empty();
  }

  pointer take(iterator pos) {
    auto res = pos.ptr;
    auto next = res->next;
    auto prev = res->prev;
    prev->next = next;
    next->prev = prev;
    return res;
  }

  iterator erase(iterator pos) {
    auto next = pos->next;
    m_delete(take(pos));
    return next;
  }

  pointer take_first_front() {
    return take(first_begin());
  }

  pointer take_second_front() {
    return take(second_begin());
  }

  value_type& first_front() {
    return *first_begin();
  }

  value_type& second_front() {
    return *second_begin();
  }

  void pop_first_front() {
    erase(first_begin());
  }

  void pop_second_front() {
    erase(second_begin());
  }

  void push_first_back(pointer val) {
    insert(first_end(), val);
  }

  void push_second_back(pointer val) {
    insert(second_end(), val);
  }

  size_t count(iterator first, iterator last, size_t max_count) {
    size_t result = 0;
    while (first != last && result < max_count) {
      ++first;
      ++result;
    }
    return result;
  }

  size_t count(size_t max_count = std::numeric_limits<size_t>::max()) {
    auto r1 = count(first_begin(), first_end(), max_count);
    return r1 + count(second_begin(), second_end(), max_count - r1);
  }

 private:
  value_type m_head;
  value_type m_separator;
  value_type m_tail;
  deleter_type m_delete;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_INTRUSIVE_LIST_HPP
