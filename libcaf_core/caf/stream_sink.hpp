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
 * http://opensink.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <tuple>
#include <typeinfo>

#include "caf/detail/type_traits.hpp"
#include "caf/inbound_path.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/rtti_pair.hpp"
#include "caf/stream_manager.hpp"
#include "caf/type_nr.hpp"

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
  inbound_stream_slot<input_type> add_inbound_path(const stream<input_type>&) {
    auto rtti = make_rtti_pair<input_type>();
    return {this->add_unchecked_inbound_path_impl(rtti)};
  }

private:
  // -- member variables -------------------------------------------------------

  downstream_manager dummy_out_;
};

template <class In>
using stream_sink_ptr = intrusive_ptr<stream_sink<In>>;

} // namespace caf

