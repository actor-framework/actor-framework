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

#ifndef CAF_DETAIL_INTRUSIVE_PARTITIONED_LIST_HPP
#define CAF_DETAIL_INTRUSIVE_PARTITIONED_LIST_HPP

#include "caf/config.hpp"

#include <memory>
#include <iterator>
#include <algorithm>

#include "caf/behavior.hpp"
#include "caf/invoke_message_result.hpp"

namespace caf {
namespace detail {

/// Describes a partitioned list of elements. The first part
/// of the list is described by the iterator pair `[begin, separator)`.
/// The second part by `[continuation, end)`. Actors use the second half
/// of the list to store previously skipped elements. Priority-aware actors
/// also use the first half of the list to sort messages by priority.
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

    iterator next() const {
      return ptr->next;
    }
  };

  intrusive_partitioned_list() {
    head_.next = &separator_;
    separator_.prev = &head_;
    separator_.next = &tail_;
    tail_.prev = &separator_;
  }

  ~intrusive_partitioned_list() {
    clear();
  }

  iterator begin() {
    return head_.next;
  }

  iterator separator() {
    return &separator_;
  }

  iterator continuation() {
    return separator_.next;
  }

  iterator end() {
    return &tail_;
  }

  using range = std::pair<iterator, iterator>;

  /// Returns the two iterator pairs describing the first and second part
  /// of the partitioned list.
  std::array<range, 2> ranges() {
    return {{range{begin(), separator()}, range{continuation(), end()}}};
  }

  template <class F>
  void clear(F f) {
    for (auto& range : ranges()) {
      auto i = range.first;
      auto e = range.second;
      while (i != e) {
        auto ptr = i.ptr;
        ++i;
        f(*ptr);
        delete_(ptr);
      }
    }
    if (head_.next != &separator_) {
      head_.next = &separator_;
      separator_.prev = &head_;
    }
    if (separator_.next != &tail_) {
      separator_.next = &tail_;
      tail_.prev = &separator_;
    }
  }

  void clear() {
    auto nop = [](value_type&) {};
    clear(nop);
  }

  iterator insert(iterator next, pointer val) {
    auto prev = next->prev;
    val->prev = prev;
    val->next = next.ptr;
    prev->next = val;
    next->prev = val;
    return val;
  }

  bool empty() const {
    return head_.next == &separator_ && separator_.next == &tail_;
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
    delete_(take(pos));
    return next;
  }

  size_t count(size_t max_count = std::numeric_limits<size_t>::max()) {
    size_t result = 0;
    for (auto& range : ranges())
      for (auto i = range.first; i != range.second; ++i)
        if (++result == max_count)
          return max_count;
    return result;
  }

private:
  value_type head_;
  value_type separator_;
  value_type tail_;
  deleter_type delete_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_INTRUSIVE_PARTITIONED_LIST_HPP
