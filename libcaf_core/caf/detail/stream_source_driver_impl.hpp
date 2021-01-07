// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/none.hpp"
#include "caf/stream_finalize_trait.hpp"
#include "caf/stream_source_driver.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf::detail {

/// Identifies an unbound sequence of messages.
template <class DownstreamManager, class Pull, class Done, class Finalize>
class stream_source_driver_impl final
  : public stream_source_driver<DownstreamManager> {
public:
  // -- member types -----------------------------------------------------------

  using super = stream_source_driver<DownstreamManager>;

  using output_type = typename super::output_type;

  using trait = stream_source_trait_t<Pull>;

  using state_type = typename trait::state;

  template <class Init>
  stream_source_driver_impl(Init init, Pull f, Done pred, Finalize fin)
    : pull_(std::move(f)), done_(std::move(pred)), fin_(std::move(fin)) {
    init(state_);
  }

  void pull(downstream<output_type>& out, size_t num) override {
    return pull_(state_, out, num);
  }

  bool done() const noexcept override {
    return done_(state_);
  }

  void finalize(const error& err) override {
    stream_finalize_trait<Finalize, state_type>::invoke(fin_, state_, err);
  }

private:
  state_type state_;
  Pull pull_;
  Done done_;
  Finalize fin_;
};

} // namespace caf::detail
