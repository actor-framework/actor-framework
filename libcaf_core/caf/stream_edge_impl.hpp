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

#ifndef CAF_STREAM_EDGE_IMPL_HPP
#define CAF_STREAM_EDGE_IMPL_HPP

#include <memory>
#include <vector>
#include <numeric>
#include <cstddef>
#include <algorithm>

#include "caf/fwd.hpp"
#include "caf/send.hpp"
#include "caf/config.hpp"
#include "caf/stream_msg.hpp"
#include "caf/actor_addr.hpp"
#include "caf/local_actor.hpp"
#include "caf/inbound_path.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_aborter.hpp"
#include "caf/actor_control_block.hpp"

namespace caf {

/// Provides a common scaffold for implementations of the `stream_gatherer` and
/// `stream_scatterer` interfaces.
template <class Base>
class stream_edge_impl : public Base {
public:
  // -- member types -----------------------------------------------------------

  using super = Base;

  /// Either `inbound_path` or `outbound_path`.
  using path_type = typename super::path_type;

  /// A raw pointer to a path.
  using path_ptr = path_type*;

  /// Vector of raw pointers (views) of paths.
  using path_ptr_vec = std::vector<path_ptr>;

  /// Iterator to a vector of raw pointers.
  using path_ptr_iter = typename path_ptr_vec::iterator;

  /// A unique pointer to a path.
  using path_uptr = std::unique_ptr<path_type>;

  /// Vector of owning pointers of paths.
  using path_uptr_vec = std::vector<path_uptr>;

  /// Iterator to a vector of owning pointers.
  using path_uptr_iter = typename path_uptr_vec::iterator;

  /// Message type for sending graceful shutdowns along the path (either
  /// `stream_msg::drop` or `stream_msg::close`).
  using regular_shutdown = typename path_type::regular_shutdown;

  /// Message type for sending errors along the path (either
  /// `stream_msg::forced_drop` or `stream_msg::forced_close`).
  using irregular_shutdown = typename path_type::irregular_shutdown;

  /// Stream aborter flag to monitor paths.
  static constexpr const auto aborter_type = path_type::aborter_type;

  // -- constructors, destructors, and assignment operators --------------------

  stream_edge_impl(local_actor* selfptr)
      : self_(selfptr),
        continuous_(false) {
    // nop
  }

  ~stream_edge_impl() override {
    // nop
  }

  // -- static utility functions for path vectors ------------------------------
  
  /// Sorts `xs` in descending order by available credit.
  template <class PathContainer>
  static void sort_by_credit(PathContainer& xs) {
    using value_type = typename PathContainer::value_type;
    auto cmp = [](const value_type& x, const value_type& y) {
      return x->open_credit > y->open_credit;
    };
    std::sort(xs.begin(), xs.end(), cmp);
  }

  template <class T, class PathContainer, class F>
  static T fold(PathContainer& xs, T init, F f) {
    auto b = xs.begin();
    auto e = xs.end();
    return b != e ? std::accumulate(b, e, init, f) : static_cast<T>(0);
  }

  /// Finds the path for `ptr` and returns a pointer to it.
  template <class PathContainer, class Handle>
  static path_ptr find(PathContainer& xs, const stream_id& sid,
                       const Handle& x) {
    auto predicate = [&](const typename PathContainer::value_type& y) {
      return y->hdl == x && y->sid == sid;
    };
    auto e = xs.end();
    auto i = std::find_if(xs.begin(), e, predicate);
    if (i != e)
      return &(*(*i)); // Ugly, but works for both raw and smart pointers.
    return nullptr;
  }

  /// Finds the path for `ptr` and returns an iterator to it.
  template <class PathContainer, class Handle>
  static typename PathContainer::iterator
  iter_find(PathContainer& xs, const stream_id& sid, const Handle& x) {
    auto predicate = [&](const typename PathContainer::value_type& y) {
      return y->hdl == x && y->sid == sid;
    };
    return std::find_if(xs.begin(), xs.end(), predicate);
  }

  // -- accessors --------------------------------------------------------------

  /// Returns all available paths.
  inline const path_uptr_vec& paths() const {
    return paths_;
  }

  /// Returns a pointer to the parent actor.
  inline local_actor* self() const {
    return self_;
  }

  // -- reusable convenience functions -----------------------------------------

  using super::remove_path;

  bool remove_path(path_uptr_iter i, error reason, bool silent) {
    CAF_LOG_TRACE(CAF_ARG(reason) << CAF_ARG(silent));
    auto e = paths_.end();
    if (i == e) {
      CAF_LOG_DEBUG("unable to remove path");
      return false;
    }
    auto& p = *(*i);
    stream_aborter::del(p.hdl, self_->address(), p.sid, aborter_type);
    if (silent)
      p.hdl = nullptr;
    if (reason != none)
      p.shutdown_reason = std::move(reason);
    if (i != paths_.end() - 1)
      std::swap(*i, paths_.back());
    paths_.pop_back();
    return true;
  }

  // -- implementation of common methods ---------------------------------------

  bool remove_path(const stream_id& sid, const actor_addr& x,
                   error reason, bool silent) override {
    CAF_LOG_TRACE(CAF_ARG(sid) << CAF_ARG(x)
                  << CAF_ARG(reason) << CAF_ARG(silent));
    return remove_path(iter_find(paths_, sid, x), std::move(reason), silent);
  }

  void abort(error reason) override {
    auto i = paths_.begin();
    auto e = paths_.end();
    if (i != e) {
      // Handle last element separately after the loop.
      --e;
      for (; i != e; ++i)
        (*i)->shutdown_reason = reason;
      (*e)->shutdown_reason = std::move(reason);
      paths_.clear();
    }
  }

  long num_paths() const override {
    return static_cast<long>(paths_.size());
  }

  bool closed() const override {
    return !continuous_ && paths_.empty();
  }

  bool continuous() const override {
    return continuous_;
  }

  void continuous(bool value) override {
    continuous_ = value;
  }

  // -- implementation of identical methods in scatterer and gatherer  ---------

  path_ptr path_at(size_t index) override {
    return paths_[index].get();
  }

  using super::find;

  path_ptr find(const stream_id& sid, const actor_addr& x) override {
    return find(paths_, sid, x);
  }

protected:
  /// Adds a path to the edge without emitting messages.
  path_ptr add_path_impl(const stream_id& sid, strong_actor_ptr x) {
    CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(sid));
    stream_aborter::add(x, self_->address(), sid, aborter_type);
    paths_.emplace_back(new path_type(self_, sid, std::move(x)));
    return paths_.back().get();
  }

  template <class F>
  void close_impl(F f) {
    for (auto& x : paths_) {
      stream_aborter::del(x->hdl, self_->address(), x->sid, aborter_type);
      f(*x);
    }
    paths_.clear();
  }

  local_actor* self_;
  path_uptr_vec paths_;
  bool continuous_;
};

} // namespace caf

#endif // CAF_STREAM_EDGE_IMPL_HPP
