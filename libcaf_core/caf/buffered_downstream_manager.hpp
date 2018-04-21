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

#include <deque>
#include <vector>
#include <cstddef>
#include <iterator>

#include "caf/downstream_manager_base.hpp"
#include "caf/logger.hpp"

namespace caf {

/// Mixin for streams with any number of downstreams. `Subtype` must provide a
/// member function `buf()` returning a queue with `std::deque`-like interface.
template <class T>
class buffered_downstream_manager : public downstream_manager_base {
public:
  // -- member types -----------------------------------------------------------

  using super = downstream_manager_base;

  using output_type = T;

  using buffer_type = std::deque<output_type>;

  using chunk_type = std::vector<output_type>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit buffered_downstream_manager(stream_manager* parent) : super(parent) {
    // nop
  }

  template <class T0, class... Ts>
  void push(T0&& x, Ts&&... xs) {
    buf_.emplace_back(std::forward<T0>(x), std::forward<Ts>(xs)...);
  }

  /// @pre `n <= buf_.size()`
  static chunk_type get_chunk(buffer_type& buf, size_t n) {
    CAF_LOG_TRACE(CAF_ARG(buf) << CAF_ARG(n));
    chunk_type xs;
    if (!buf.empty() && n > 0) {
      xs.reserve(std::min(n, buf.size()));
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

  chunk_type get_chunk(size_t n) {
    return get_chunk(buf_, n);
  }

  bool terminal() const noexcept override {
    return false;
  }

  size_t capacity() const noexcept override {
    // Our goal is to cache up to 2 full batches, whereby we pick the highest
    // batch size available to us (optimistic estimate).
    size_t desired = 1;
    for (auto& kvp : this->paths_)
      desired = std::max(static_cast<size_t>(kvp.second->desired_batch_size),
                         desired);
    desired *= 2;
    auto stored = buffered();
    return stored < desired ? desired - stored : 0u;
  }

  size_t buffered() const noexcept override {
    return buf_.size();
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

} // namespace caf

