// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/none.hpp"
#include "caf/stream_finalize_trait.hpp"
#include "caf/stream_sink_driver.hpp"
#include "caf/stream_sink_trait.hpp"

namespace caf::detail {

/// Identifies an unbound sequence of messages.
template <class Input, class Process, class Finalize>
class stream_sink_driver_impl final : public stream_sink_driver<Input> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_sink_driver<Input>;

  using typename super::input_type;

  using trait = stream_sink_trait_t<Process>;

  using state_type = typename trait::state;

  template <class Init>
  stream_sink_driver_impl(Init init, Process f, Finalize fin)
    : process_(std::move(f)), fin_(std::move(fin)) {
    init(state_);
  }

  void process(std::vector<input_type>& xs) override {
    return trait::process::invoke(process_, state_, xs);
  }

  void finalize(const error& err) override {
    stream_finalize_trait<Finalize, state_type>::invoke(fin_, state_, err);
  }

private:
  Process process_;
  Finalize fin_;
  state_type state_;
};

} // namespace caf::detail
