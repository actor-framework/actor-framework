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

#include <tuple>

#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/stream_sink.hpp"
#include "caf/stream_source.hpp"

namespace caf {

template <class In, class DownstreamManager>
class stream_stage : public stream_source<DownstreamManager>,
                     public stream_sink<In> {
public:
  // -- member types -----------------------------------------------------------

  using left_super = stream_source<DownstreamManager>;

  using right_super = stream_sink<In>;

  // -- constructors, destructors, and assignment operators --------------------

  stream_stage(scheduled_actor* self)
      : stream_manager(self),
        left_super(self),
        right_super(self) {
    // nop
  }

  // -- overridden member functions --------------------------------------------

  bool done() const override {
    return !this->continuous() && this->inbound_paths_.empty()
           && this->pending_handshakes_ == 0 && this->out_.clean();
  }

  bool idle() const noexcept override {
    // A stage is idle if it can't make progress on its downstream manager or
    // if it has no pending work at all.
    auto& dm = this->out_;
    return dm.stalled() || (dm.clean() && right_super::idle());
  }

  DownstreamManager& out() override {
    return left_super::out();
  }
};

template <class In, class DownstreamManager>
using stream_stage_ptr = intrusive_ptr<stream_stage<In, DownstreamManager>>;

} // namespace caf

