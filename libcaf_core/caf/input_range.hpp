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

#pragma once

#include <iterator>

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class T>
class input_range {
public:
  virtual ~input_range() {
    // nop
  }

  input_range() = default;
  input_range(const input_range&) = default;
  input_range& operator=(const input_range&) = default;

  class iterator : public std::iterator<std::input_iterator_tag, T> {
  public:
    iterator(input_range* range) : xs_(range) {
      if (xs_)
        advance();
      else
        x_ = nullptr;
    }

    iterator(const iterator&) = default;
    iterator& operator=(const iterator&) = default;

    bool operator==(const iterator& other) const {
      return xs_ == other.xs_;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    T& operator*() {
      return *x_;
    }

    T* operator->() {
      return x_;
    }

    iterator& operator++() {
      advance();
      return *this;
    }

    iterator operator++(int) {
      iterator copy{xs_};
      advance();
      return copy;
    }

  private:
    void advance() {
      x_ = xs_->next();
      if (!x_)
        xs_ = nullptr;
    }

    input_range* xs_;
    T* x_;
  };

  virtual T* next() = 0;

  iterator begin() {
    return this;
  }

  iterator end() const {
    return nullptr;
  }
};

template <class I>
class input_range_impl : public input_range<detail::value_type_of_t<I>> {
public:
  using value_type = detail::value_type_of_t<I>;

  input_range_impl(I first, I last) : pos_(first), last_(last) {
    // nop
  }

  value_type* next() override {
    return (pos_ == last_) ? nullptr : &(*pos_++);
  }

private:
  I pos_;
  I last_;
};

/// @relates input_range
template <class I>
input_range_impl<I> make_input_range(I first, I last) {
  return {first, last};
}

} // namespace caf

