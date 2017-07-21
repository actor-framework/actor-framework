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

#ifndef CAF_TOPIC_SCATTERER_HPP
#define CAF_TOPIC_SCATTERER_HPP

#include <map>
#include <tuple>
#include <deque>
#include <vector>
#include <functional>

#include "caf/buffered_scatterer.hpp"

#include "caf/meta/type_name.hpp"

namespace caf {

/// A topic scatterer allows stream nodes to fork into multiple lanes, where
/// each lane carries only a subset of the data. For example, the lane
/// mechanism allows you filter key/value pairs before forwarding them to a set
/// of workers.
template <class T, class Filter, class KeyCompare, long KeyIndex>
class topic_scatterer : public buffered_scatterer<T> {
public:
  /// Base type.
  using super = buffered_scatterer<T>;

  struct lane {
    typename super::buffer_type buf;
    typename super::path_ptr_vec paths;
    template <class Inspector>
    friend typename Inspector::result_type inspect(Inspector& f, lane& x) {
      return f(meta::type_name("lane"), x.buf, x.paths);
    }
  };

  /// Identifies a lane inside the downstream.
  using filter_type = Filter;

  using lanes_map = std::map<filter_type, lane>;

  topic_scatterer(local_actor* selfptr) : super(selfptr) {
    // nop
  }

  using super::remove_path;

  bool remove_path(const stream_id& sid, const actor_addr& x,
                   error reason, bool silent) override {
    auto i = this->iter_find(this->paths_, sid, x);
    if (i != this->paths_.end()) {
      erase_from_lanes(x);
      return super::remove_path(i, std::move(reason), silent);
    }
    return false;
  }

  void add_lane(filter_type f) {
    std::sort(f);
    lanes_.emplace(std::move(f), typename super::buffer_type{});
  }

  /// Sets the filter for `x` to `f` and inserts `x` into the appropriate lane.
  /// @pre `x` is not registered on *any* lane
  template <class Handle>
  void set_filter(const stream_id& sid, const Handle& x, filter_type f) {
    std::sort(f.begin(), f.end());
    lanes_[std::move(f)].paths.push_back(super::find(sid, x));
  }

  template <class Handle>
  void update_filter(const stream_id& sid, const Handle& x, filter_type f) {
    std::sort(f.begin(), f.end());
    erase_from_lanes(x);
    lanes_[std::move(f)].paths.push_back(super::find(sid, x));
  }

  const lanes_map& lanes() const {
    return lanes_;
  }

protected:
  template <class Handle>
  void erase_from_lanes(const Handle& x) {
    for (auto i = lanes_.begin(); i != lanes_.end(); ++i)
      if (erase_from_lane(i->second, x)) {
        if (i->second.paths.empty())
          lanes_.erase(i);
        return;
      }
  }

  template <class Handle>
  bool erase_from_lane(lane& l, const Handle& x) {
    auto predicate = [&](const outbound_path* y) {
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
          kvp.second.buf.push_back(x);
    this->buf_.clear();
  }

  /// Returns `true` if `x` is selected by `f`, `false` otherwise.
  bool selected(const filter_type& f, const T& x) {
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

#endif // CAF_TOPIC_SCATTERER_HPP
