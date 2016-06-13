/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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
  /// Topic filters defined by a downstream actor.
  using topics = abstract_downstream::topics;

  using topics_set = abstract_downstream::topics_set;

  /// A chunk of data for sending batches downstream.
  using chunk = std::vector<T>;

  /// Chunks split into multiple categories via filters.
  using categorized_chunks = std::unordered_map<topics, chunk>;

  downstream(local_actor* ptr, const stream_id& sid,
             typename abstract_downstream::policy_ptr policy)
      : abstract_downstream(ptr, sid, std::move(policy)) {
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

  void broadcast() override {

    auto chunk = get_chunk(min_credit());
    auto csize = chunk.size();
    if (csize == 0)
      return;
    auto wrapped_chunk = make_message(std::move(chunk));
    for (auto& x : paths_) {
      x->open_credit -= csize;
      send_batch(*x, csize, wrapped_chunk);
    }
  }

  void anycast() override {
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

  /// @pre `n <= buf_.size()`
  chunk get_chunk(size_t n, const topics& filter) {
    if (filter.empty())
      return get_chunk(n);
    chunk xs;
    if (n > 0) {
      xs.reserve(n);
      auto in_filter = [&](atom_value x) {
        auto e = filter.end();
        return std::find(filter.begin(), e, x) != e;
      };
      // temporarily stores a T with the result of in_filter()
      scratch_space ys;
      move_from_buf_until_nth_match(ys, n, in_filter);
      auto is_marked = [](const scratch_space_value& x) {
        return std::get<1>(x);
      };
      // helper for iterating over ys as if using a move iterator
      // over vector<T>
      // parition range into result messages and message to move back into buf_
      scratch_space_move_iter first{ys.begin()};
      scratch_space_move_iter last{ys.end()};
      scratch_space_move_iter pp{std::stable_partition(first.i, last.i,
                                                       is_marked)};
      xs.insert(xs.end(), first, pp);
      buf_.insert(buf_.begin(), pp, last);
    }
    return std::move(xs);
  }

  categorized_chunks get_chunks(size_t n, const topics_set& filters) {
    categorized_chunks res;
    if (filters.empty()) {
      res.emplace(topics{}, get_chunk(n));
    } else if (filters.size() == 1) {
      res.emplace(*filters.begin(), get_chunk(n, *filters.begin()));
    } else {
      // reserve sufficient space for chunks
      for (auto& filter : filters) {
        chunk tmp;
        tmp.reserve(n);
        res.emplace(filter, std::move(tmp));
      }
      // get up to n elements from buffer
      auto in_filter = [](const topics_set& filter, atom_value x) {
        auto e = filter.end();
        return filter.empty() || std::find(filter.begin(), e, x) != e;
      };
      auto in_any_filter = [&](atom_value x) {
        for (auto& filter : filters)
          if (in_filter(filter, x))
            return true;
        return false;
      };
      scratch_space ys;
      move_from_buf_until_nth_match(ys, n, in_any_filter);
      // parition range into result messages and message to move back into buf_
      auto is_marked = [](const scratch_space_value& x) {
        return std::get<1>(x);
      };
      auto first = ys.begin();
      auto last = ys.end();
      auto pp = std::stable_partition(first, last, is_marked);
      // copy matched values into according chunks
      for (auto& r : res)
        for (auto& y : ys)
          if (in_filter(r.first, std::get<2>(y)))
            r.second.emplace_back(std::get<0>(y));
      // move unused items back into place
      buf_.insert(buf_.begin(), scratch_space_move_iter{pp},
                  scratch_space_move_iter{last});
    }
    return res;
  }

  // moves elements from `buf_` to `tmp` until n elements matching the
  // predicate were moved or the buffer was fully consumed. 
  // @pre `n > 0`
  template <class F>
  void move_from_buf_until_nth_match(scratch_space& ys, size_t n, F pred) {
    CAF_ASSERT(n > 0);
    size_t included = 0; // nr. of included elements
    for (auto i = buf_.begin(); i != buf_.end(); ++i) {
      auto topic = this->policy().categorize(*i);
      auto match = pred(topic);
      ys.emplace_back(std::move(*i), match, topic);
      if (match && ++included == n) {
        buf_.erase(buf_.begin(), i + 1);
        return;
      }
    }
    // consumed whole buffer
    buf_.clear();
  }

  std::deque<T> buf_;
};

} // namespace caf

#endif // CAF_DOWNSTREAM_HPP
