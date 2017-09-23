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

#include "caf/make_message.hpp"

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

  // @private
  queue_type& buf() {
    return buf_;
  }

protected:
  queue_type& buf_;
};

} // namespace caf

#endif // CAF_DOWNSTREAM_HPP
