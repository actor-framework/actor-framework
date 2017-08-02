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
    return this->min_credit() + this->min_buffer_size();
  }

  void emit_batches() override {
    CAF_LOG_TRACE("");
    auto chunk = this->get_chunk(this->min_credit());
    auto csize = static_cast<long>(chunk.size());
    CAF_LOG_TRACE(CAF_ARG(chunk));
    if (csize == 0)
      return;
    auto wrapped_chunk = make_message(std::move(chunk));
    for (auto& x : this->paths_) {
      CAF_ASSERT(x->open_credit >= csize);
      x->emit_batch(csize, wrapped_chunk);
    }
  }
};

} // namespace caf

#endif // CAF_BROADCAST_SCATTERER_HPP
