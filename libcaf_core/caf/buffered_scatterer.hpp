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

#ifndef CAF_BUFFERED_SCATTERER_HPP
#define CAF_BUFFERED_SCATTERER_HPP

#include <deque>
#include <vector>
#include <cstddef>
#include <iterator>

#include "caf/actor_control_block.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"
#include "caf/stream_scatterer.hpp"

namespace caf {

/// Mixin for streams with any number of downstreams. `Subtype` must provide a
/// member function `buf()` returning a queue with `std::deque`-like interface.
template <class T>
class buffered_scatterer : public stream_scatterer {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_scatterer;

  using value_type = T;

  using buffer_type = std::deque<value_type>;

  using chunk_type = std::vector<value_type>;

  // -- constructors, destructors, and assignment operators --------------------

  buffered_scatterer(local_actor* self) : super(self) {
    // nop
  }

  template <class T0, class... Ts>
  void push(T0&& x, Ts&&... xs) {
    buf_.emplace_back(std::forward<T0>(x), std::forward<Ts>(xs)...);
  }

  /// @pre `n <= buf_.size()`
  static chunk_type get_chunk(buffer_type& buf, long n) {
    CAF_LOG_TRACE(CAF_ARG(buf) << CAF_ARG(n));
    chunk_type xs;
    if (!buf.empty() && n > 0) {
      xs.reserve(std::min(static_cast<size_t>(n), buf.size()));
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

  size_t capacity() const noexcept override {
    // TODO: get rid of magic number
    static constexpr size_t max_buf_size = 100;
    return buf_.size() < max_buf_size ? max_buf_size - buf_.size() : 0u;
  }

  size_t buffered() const noexcept override {
    return buf_.size();
  }

  message make_handshake_token(stream_slot slot) const override {
    return make_message(stream<T>{slot});
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

#endif // CAF_BUFFERED_SCATTERER_HPP
