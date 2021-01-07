// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
    : stream_manager(self), left_super(self), right_super(self) {
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
    return this->out_;
  }
};

template <class In, class DownstreamManager>
using stream_stage_ptr = intrusive_ptr<stream_stage<In, DownstreamManager>>;

} // namespace caf
