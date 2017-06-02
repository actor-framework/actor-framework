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

#ifndef CAF_POLICY_BROADCAST_HPP
#define CAF_POLICY_BROADCAST_HPP

#include "caf/downstream_policy.hpp"

#include "caf/mixin/buffered_policy.hpp"

namespace caf {
namespace policy {

template <class T, class Base = mixin::buffered_policy<T, downstream_policy>>
class broadcast : public Base {
public:
  template <class... Ts>
  broadcast(Ts&&... xs) : Base(std::forward<Ts>(xs)...) {
    // nop
  }

  void emit_batches() override {
    this->emit_broadcast();
  }

  long credit() const override {
    // We receive messages until we have exhausted all downstream credit and
    // have filled our buffer to its minimum size.
    return this->min_credit() + this->min_buffer_size();
  }
};

} // namespace policy
} // namespace caf

#endif // CAF_POLICY_BROADCAST_HPP
