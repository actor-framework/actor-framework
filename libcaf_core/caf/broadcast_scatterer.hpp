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

#ifndef CAF_BROADCAST_SCATTERER_HPP
#define CAF_BROADCAST_SCATTERER_HPP

#include "caf/buffered_scatterer.hpp"

namespace caf {

template <class T>
class broadcast_scatterer : public buffered_scatterer<T> {
public:
  using super = buffered_scatterer<T>;

  broadcast_scatterer(local_actor* selfptr) : super(selfptr) {
    // nop
  }

  long credit() const override {
    // We receive messages until we have exhausted all downstream credit and
    // have filled our buffer to its minimum size.
    return this->min_credit();
  }

  void emit_batches() override {
    CAF_LOG_TRACE("");
    auto hint = this->min_desired_batch_size();
    auto next_chunk = [&] {
      // TODO: this iterates paths_ every time again even though we could
      //       easily keep track of remaining credit
      return this->get_chunk(std::min(this->min_credit(), hint));
    };
    for (auto chunk = next_chunk(); !chunk.empty(); chunk = next_chunk()) {
      auto csize = static_cast<long>(chunk.size());
      auto wrapped_chunk = make_message(std::move(chunk));
      for (auto& x : this->paths_) {
        CAF_ASSERT(x->open_credit >= csize);
        x->emit_batch(csize, wrapped_chunk);
      }
    }
  }

  long desired_batch_size() const override {
    // TODO: this is an O(n) computation, consider storing the result in a
    //       member variable for
    return super::min_desired_batch_size();
  }
};

} // namespace caf

#endif // CAF_BROADCAST_SCATTERER_HPP
