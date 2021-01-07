// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <algorithm>
#include <tuple>
#include <typeinfo>

#include "caf/detail/type_traits.hpp"
#include "caf/downstream_manager.hpp"
#include "caf/inbound_path.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/stream_manager.hpp"

namespace caf {

template <class In>
class stream_sink : public virtual stream_manager {
public:
  // -- member types -----------------------------------------------------------

  using input_type = In;

  // -- constructors, destructors, and assignment operators --------------------

  stream_sink(scheduled_actor* self) : stream_manager(self), dummy_out_(this) {
    // nop
  }

  // -- overridden member functions --------------------------------------------

  bool done() const override {
    return !this->continuous() && this->inbound_paths_.empty();
  }

  bool idle() const noexcept override {
    // A sink is idle if there's no pending batch and a new credit round would
    // emit no `ack_batch` messages.
    return this->inbound_paths_idle();
  }

  downstream_manager& out() override {
    return dummy_out_;
  }

  // -- properties -------------------------------------------------------------

  /// Creates a new input path to the current sender.
  inbound_stream_slot<input_type> add_inbound_path(stream<input_type> in) {
    return {this->add_unchecked_inbound_path(in)};
  }

private:
  // -- member variables -------------------------------------------------------

  downstream_manager dummy_out_;
};

template <class In>
using stream_sink_ptr = intrusive_ptr<stream_sink<In>>;

} // namespace caf
