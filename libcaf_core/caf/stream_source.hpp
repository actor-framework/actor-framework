// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <tuple>

#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/logger.hpp"
#include "caf/outbound_path.hpp"
#include "caf/stream_manager.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class DownstreamManager>
class stream_source : public virtual stream_manager {
public:
  // -- member types -----------------------------------------------------------

  using output_type = typename DownstreamManager::output_type;

  // -- constructors, destructors, and assignment operators --------------------

  stream_source(scheduled_actor* self) : stream_manager(self), out_(this) {
    // nop
  }

  bool idle() const noexcept override {
    // A source is idle if it can't make any progress on its downstream or if
    // it's not producing new data despite having credit.
    auto some_credit = [](const outbound_path& x) { return x.open_credit > 0; };
    return out_.stalled()
           || (out_.buffered() == 0 && out_.all_paths(some_credit));
  }

  DownstreamManager& out() override {
    return out_;
  }

  /// Creates a new output path to the current sender.
  outbound_stream_slot<output_type> add_outbound_path() {
    CAF_LOG_TRACE("");
    return this->add_unchecked_outbound_path<output_type>();
  }

  /// Creates a new output path to the current sender with custom handshake.
  template <class... Ts>
  outbound_stream_slot<output_type, detail::strip_and_convert_t<Ts>...>
  add_outbound_path(std::tuple<Ts...> xs) {
    CAF_LOG_TRACE(CAF_ARG(xs));
    return this->add_unchecked_outbound_path<output_type>(std::move(xs));
  }

  /// Creates a new output path to the current sender.
  template <class Handle>
  outbound_stream_slot<output_type> add_outbound_path(const Handle& next) {
    CAF_LOG_TRACE(CAF_ARG(next));
    return this->add_unchecked_outbound_path<output_type>(next);
  }

  /// Creates a new output path to the current sender with custom handshake.
  template <class Handle, class... Ts>
  outbound_stream_slot<output_type, detail::strip_and_convert_t<Ts>...>
  add_outbound_path(const Handle& next, std::tuple<Ts...> xs) {
    CAF_LOG_TRACE(CAF_ARG(next) << CAF_ARG(xs));
    return this->add_unchecked_outbound_path<output_type>(next, std::move(xs));
  }

protected:
  DownstreamManager out_;
};

template <class DownstreamManager>
using stream_source_ptr = intrusive_ptr<stream_source<DownstreamManager>>;

} // namespace caf
