// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <deque>
#include <vector>

#include "caf/message.hpp"

namespace caf {

/// Grants access to an output stream buffer.
template <class T>
class downstream {
public:
  // -- member types -----------------------------------------------------------

  /// A queue of items for temporary storage before moving them into chunks.
  using queue_type = std::deque<T>;

  // -- constructors, destructors, and assignment operators --------------------

  downstream(queue_type& q) : buf_(q) {
    // nop
  }

  // -- queue access -----------------------------------------------------------

  template <class... Ts>
  void push(Ts&&... xs) {
    buf_.emplace_back(std::forward<Ts>(xs)...);
  }

  template <class Iterator, class Sentinel>
  void append(Iterator first, Sentinel last) {
    buf_.insert(buf_.end(), first, last);
  }

  // @private
  queue_type& buf() {
    return buf_;
  }

protected:
  queue_type& buf_;
};

} // namespace caf
