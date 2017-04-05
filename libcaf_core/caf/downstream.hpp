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

#ifndef CAF_DOWNSTREAM_HPP
#define CAF_DOWNSTREAM_HPP

#include <deque>
#include <vector>

#include "caf/stream_msg.hpp"
#include "caf/downstream_path.hpp"
#include "caf/abstract_downstream.hpp"

namespace caf {

template <class T>
class downstream final : public abstract_downstream {
public:
  /// A chunk of data for sending batches downstream.
  using chunk = std::vector<T>;

  downstream(local_actor* ptr, const stream_id& sid,
             typename abstract_downstream::policy_ptr pptr)
      : abstract_downstream(ptr, sid, std::move(pptr)) {
    // nop
  }

  template <class... Ts>
  void push(Ts&&... xs) {
    buf_.emplace_back(std::forward<Ts>(xs)...);
  }

  std::deque<T>& buf() {
    return buf_;
  }

  const std::deque<T>& buf() const {
    return buf_;
  }

  void broadcast(size_t* hint) override {
    auto chunk = get_chunk(hint ? *hint : min_credit());
    auto csize = chunk.size();
    if (csize == 0)
      return;
    auto wrapped_chunk = make_message(std::move(chunk));
    for (auto& x : paths_) {
      x->open_credit -= csize;
      send_batch(*x, csize, wrapped_chunk);
    }
  }

  void anycast(size_t*) override {
    sort_by_credit();
    for (auto& x : paths_) {
      auto chunk = get_chunk(x->open_credit);
      auto csize = chunk.size();
      if (csize == 0)
        return;
      x->open_credit -= csize;
      auto wrapped_chunk = make_message(std::move(chunk));
      send_batch(*x, csize, std::move(wrapped_chunk));
    }
  }

  size_t buf_size() const override {
    return buf_.size();
  }

private:
  /// Returns the minimum of `desired` and `buf_.size()`.
  size_t clamp_chunk_size(size_t desired) const {
    return std::min(desired, buf_.size());
  }

  using scratch_space_value = std::tuple<T, bool, atom_value>;

  using scratch_space = std::vector<scratch_space_value>;

  /// Iterator over a `scratch_space` that behaves like a move
  /// iterator over `vector<T>`.
  struct scratch_space_move_iter {
    typename scratch_space::iterator i;

    scratch_space_move_iter& operator++() {
      ++i;
      return *this;
    }

    T&& operator*() {
      return std::move(std::get<0>(*i));
    }

    bool operator!=(const scratch_space_move_iter& x) {
      return i != x.i;
    }
  };

  /// @pre `n <= buf_.size()`
  chunk get_chunk(size_t n) {
    chunk xs;
    if (n > 0) {
      xs.reserve(n);
      if (n < buf_.size()) {
        auto first = buf_.begin();
        auto last = first + static_cast<ptrdiff_t>(n);
        std::move(first, last, std::back_inserter(xs));
        buf_.erase(first, last);
      } else {
        std::move(buf_.begin(), buf_.end(), std::back_inserter(xs));
        buf_.clear();
      }
    }
    return xs;
  }

  std::deque<T> buf_;
};

} // namespace caf

#endif // CAF_DOWNSTREAM_HPP
