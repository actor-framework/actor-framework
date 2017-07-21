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

namespace caf {

/// A topic scatterer that delivers data in broadcast fashion to all sinks.
template <class T, class Filter,
          class KeyCompare = std::equal_to<typename Filter::value_type>,
          long KeyIndex = 0>
class broadcast_topic_scatterer
    : public topic_scatterer<T, Filter, KeyCompare, KeyIndex> {
public:
  /// Base type.
  using super = buffered_scatterer<T>;

  broadcast_topic_scatterer(local_actor* selfptr) : super(selfptr) {
    // nop
  }

  long credit() const override {
    // We receive messages until we have exhausted all downstream credit and
    // have filled our buffer to its minimum size.
    return this->min_credit() + this->min_buffer_size();
  }

  void emit_batches() override {
    this->fan_out();
    for (auto& kvp : this->lanes_) {
      auto& l = kvp.second;
      auto chunk = super::get_chunk(l.buf, super::min_credit(l.paths));
      auto csize = static_cast<long>(chunk.size());
      if (csize == 0)
        continue;
      auto wrapped_chunk = make_message(std::move(chunk));
      for (auto& x : l.paths) {
        x->open_credit -= csize;
        this->emit_batch(*x, static_cast<size_t>(csize), wrapped_chunk);
      }
    }
  }
};

} // namespace caf

#endif // CAF_TOPIC_SCATTERER_HPP
