// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <deque>
#include <iterator>
#include <vector>

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

  // -- sanity checks ----------------------------------------------------------

  static_assert(detail::is_complete<type_id<std::vector<output_type>>>);

  // -- constructors, destructors, and assignment operators --------------------

  explicit buffered_downstream_manager(stream_manager* parent) : super(parent) {
    // nop
  }

  buffered_downstream_manager(stream_manager* parent, type_id_t type)
    : super(parent, type) {
    // nop
  }

  template <class T0, class... Ts>
  void push(T0&& x, Ts&&... xs) {
    buf_.emplace_back(std::forward<T0>(x), std::forward<Ts>(xs)...);
    this->generated_messages(1);
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
    return desired > stored ? desired - stored : 0u;
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
