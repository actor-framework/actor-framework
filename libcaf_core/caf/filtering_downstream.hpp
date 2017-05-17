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

#ifndef CAF_FILTERING_DOWNSTREAM_HPP
#define CAF_FILTERING_DOWNSTREAM_HPP

#include <map>
#include <tuple>
#include <deque>
#include <vector>
#include <functional>

#include "caf/downstream.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// A filtering downstream allows stages to fork into multiple lanes, where
/// each lane carries only a subset of the data. For example, the lane
/// mechanism allows you filter key/value pairs before forwarding them to a set
/// of workers in order to handle only a subset of the overall data on each
/// lane.
template <class T, class Key, class KeyCompare = std::equal_to<Key>,
          long KeyIndex = 0>
class filtering_downstream : public downstream<T> {
public:
  /// Base type.
  using super = downstream<T>;

  struct lane {
    typename super::queue_type queue;
    typename super::path_ptr_list paths;
    template <class Inspector>
    friend typename Inspector::result_type inspect(Inspector& f, lane& x) {
      return f(meta::type_name("lane"), x.queue, x.paths);
    }
  };

  /// Identifies a lane inside the downstream. Filters are kept in sorted order
  /// and require `Key` to provide `operator<`.
  using filter = std::vector<Key>;

  using lanes_map = std::map<filter, lane>;

  filtering_downstream(local_actor* ptr, const stream_id& sid,
                       typename abstract_downstream::policy_ptr pptr)
      : super(ptr, sid, std::move(pptr)) {
    // nop
  }

  void broadcast(long* hint) override {
    fan_out();
    for (auto& kvp : lanes_) {
      auto& l = kvp.second;
      auto chunk = super::get_chunk(l.queue,
                                    hint ? *hint : super::min_credit(l.paths));
      auto csize = static_cast<long>(chunk.size());
      if (csize == 0)
        continue;
      auto wrapped_chunk = make_message(std::move(chunk));
      for (auto& x : l.paths) {
        x->open_credit -= csize;
        super::send_batch(*x, csize, wrapped_chunk);
      }
    }
  }

  void anycast(long*) override {
    fan_out();
    for (auto& kvp : lanes_) {
      auto& l = kvp.second;
      super::sort_by_credit(l.paths);
      for (auto& x : l.paths) {
        auto chunk = super::get_chunk(l.queue, x->open_credit);
        auto csize = static_cast<long>(chunk.size());
        if (csize == 0)
          break;
        x->open_credit -= csize;
        super::send_batch(*x, csize, std::move(make_message(std::move(chunk))));
      }
    }
  }

  void add_lane(filter f) {
    std::sort(f);
    lanes_.emplace(std::move(f), typename super::queue_type{});
  }

  /// Sets the filter for `x` to `f` and inserts `x` into the appropriate lane.
  /// @pre `x` is not registered on *any* lane
  void set_filter(const strong_actor_ptr& x, filter f) {
    std::sort(f.begin(), f.end());
    lanes_[std::move(f)].paths.push_back(super::find(x));
  }

  void update_filter(const strong_actor_ptr& x, filter f) {
    std::sort(f.begin(), f.end());
    erase_from_lanes(x);
    lanes_[std::move(f)].paths.push_back(super::find(x));
  }

  const lanes_map& lanes() const {
    return lanes_;
  }

private:
  void erase_from_lanes(const strong_actor_ptr& x) {
    for (auto i = lanes_.begin(); i != lanes_.end(); ++i)
      if (erase_from_lane(i->second, x)) {
        if (i->second.empty())
          lanes_.erase(i);
        return;
      }
  }
  bool erase_from_lane(lane& l, const strong_actor_ptr& x) {
    auto predicate = [&](const typename super::path* y) {
      return x == y->hdl;
    };
    auto e = l.paths.end();
    auto i = std::find_if(l.paths.begin(), e, predicate);
    if (i != e) {
      l.paths.erase(i);
      return true;
    }
    return false;
  }

  /// Spreads the content of `buf_` to `lanes_`.
  void fan_out() {
    for (auto& kvp : lanes_)
      for (auto& x : this->buf_)
        if (selected(kvp.first, x))
          kvp.second.queue.push_back(x);
    this->buf_.clear();
  }

  /// Returns `true` if `x` is selected by `f`, `false` otherwise.
  bool selected(const filter& f, const T& x) {
    using std::get;
    for (auto& key : f)
      if (cmp_(key, get<KeyIndex>(x)))
        return true;
    return false;
  }

  lanes_map lanes_;
  KeyCompare cmp_;
};

} // namespace caf

#endif // CAF_FILTERING_DOWNSTREAM_HPP
