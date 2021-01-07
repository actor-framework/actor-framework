// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/stream_finalize_trait.hpp"
#include "caf/stream_slot.hpp"
#include "caf/stream_stage_driver.hpp"
#include "caf/stream_stage_trait.hpp"

namespace caf::detail {

/// Default implementation for a `stream_stage_driver` that hardwires `message`
/// as result type and implements `process` and `finalize` using user-provided
/// function objects (usually lambdas).
template <class Input, class DownstreamManager, class Process, class Finalize>
class stream_stage_driver_impl final
  : public stream_stage_driver<Input, DownstreamManager> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_stage_driver<Input, DownstreamManager>;

  using typename super::input_type;

  using typename super::output_type;

  using typename super::stream_type;

  using trait = stream_stage_trait_t<Process>;

  using state_type = typename trait::state;

  template <class Init>
  stream_stage_driver_impl(DownstreamManager& out, Init init, Process f,
                           Finalize fin)
    : super(out), process_(std::move(f)), fin_(std::move(fin)) {
    init(state_);
  }

  void process(downstream<output_type>& out,
               std::vector<input_type>& batch) override {
    trait::process::invoke(process_, state_, out, batch);
  }

  void finalize(const error& err) override {
    stream_finalize_trait<Finalize, state_type>::invoke(fin_, state_, err);
  }

private:
  state_type state_;
  Process process_;
  Finalize fin_;
};

} // namespace caf::detail
