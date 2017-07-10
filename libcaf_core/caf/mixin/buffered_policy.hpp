/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_MIXIN_BUFFERED_POLICYS_HPP
#define CAF_MIXIN_BUFFERED_POLICYS_HPP

#include <deque>
#include <vector>
#include <cstddef>
#include <iterator>

#include "caf/sec.hpp"
#include "caf/logger.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {
namespace mixin {

/// Mixin for streams with any number of downstreams. `Subtype` must provide a
/// member function `buf()` returning a queue with `std::deque`-like interface.
template <class T, class Base, class Subtype = Base>
class buffered_policy : public Base {
public:
  using value_type = T;

  using buffer_type = std::deque<value_type>;

  using chunk_type = std::vector<value_type>;

  template <class... Ts>
  buffered_policy(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }

  template <class T0, class... Ts>
  void push(T0&& x, Ts&&... xs) {
    buf_.emplace_back(std::forward<T0>(x), std::forward<Ts>(xs)...);
  }

  /// @pre `n <= buf_.size()`
  static chunk_type get_chunk(buffer_type& buf, long n) {
    chunk_type xs;
    if (n > 0) {
      xs.reserve(static_cast<size_t>(n));
      if (static_cast<size_t>(n) < buf.size()) {
        auto first = buf.begin();
        auto last = first + static_cast<ptrdiff_t>(n);
        std::move(first, last, std::back_inserter(xs));
        buf.erase(first, last);
      } else {
        std::move(buf.begin(), buf.end(), std::back_inserter(xs));
        buf.clear();
      }
    }
    return xs;
  }

  chunk_type get_chunk(long n) {
    return get_chunk(buf_, n);
  }

  long buf_size() const override {
    return static_cast<long>(buf_.size());
  }

  void emit_broadcast() override {
    auto chunk = get_chunk(this->min_credit());
    auto csize = static_cast<long>(chunk.size());
    CAF_LOG_TRACE(CAF_ARG(chunk));
    if (csize == 0)
      return;
    auto wrapped_chunk = make_message(std::move(chunk));
    for (auto& x : this->paths_) {
      CAF_ASSERT(x->open_credit >= csize);
      x->open_credit -= csize;
      this->emit_batch(*x, static_cast<size_t>(csize), wrapped_chunk);
    }
  }

  void emit_anycast() override {
    this->sort_paths_by_credit();
    for (auto& x : this->paths_) {
      auto chunk = get_chunk(x->open_credit);
      auto csize = chunk.size();
      if (csize == 0)
        return;
      x->open_credit -= csize;
      this->emit_batch(*x, csize, std::move(make_message(std::move(chunk))));
    }
  }

  buffer_type& buf() {
    return buf_;
  }

  const buffer_type& buf() const {
    return buf_;
  }

protected:
  buffer_type buf_;
};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_BUFFERED_POLICYS_HPP
