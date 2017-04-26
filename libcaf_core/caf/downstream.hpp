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
class downstream : public abstract_downstream {
public:
  /// A chunk of data for sending batches downstream.
  using chunk_type = std::vector<T>;

  /// A queue of items for temporary storage before moving them into chunks.
  using queue_type = std::deque<T>;

  downstream(local_actor* ptr, const stream_id& sid,
             typename abstract_downstream::policy_ptr pptr)
      : abstract_downstream(ptr, sid, std::move(pptr)) {
    // nop
  }

  template <class... Ts>
  void push(Ts&&... xs) {
    buf_.emplace_back(std::forward<Ts>(xs)...);
  }

  queue_type& buf() {
    return buf_;
  }

  const queue_type& buf() const {
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
    this->sort_by_credit();
    for (auto& x : paths_) {
      auto chunk = get_chunk(x->open_credit);
      auto csize = chunk.size();
      if (csize == 0)
        return;
      x->open_credit -= csize;
      send_batch(*x, csize, std::move(make_message(std::move(chunk))));
    }
  }

  size_t buf_size() const override {
    return buf_.size();
  }

protected:
  /// @pre `n <= buf_.size()`
  static chunk_type get_chunk(queue_type& buf, size_t n) {
    chunk_type xs;
    if (n > 0) {
      xs.reserve(n);
      if (n < buf.size()) {
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

  /// @pre `n <= buf_.size()`
  chunk_type get_chunk(size_t n) {
    return get_chunk(buf_, n);
  }

  queue_type buf_;
};

} // namespace caf

#endif // CAF_DOWNSTREAM_HPP
