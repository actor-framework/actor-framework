/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include "caf/detail/stream_sink_driver_impl.hpp"
#include "caf/detail/stream_sink_impl.hpp"
#include "caf/fwd.hpp"
#include "caf/make_sink_result.hpp"
#include "caf/policy/arg.hpp"
#include "caf/stream.hpp"
#include "caf/stream_sink.hpp"

namespace caf {

/// Attaches a new stream sink to `self` by creating a default stream sink /
/// manager from given callbacks.
/// @param self Points to the hosting actor.
/// @param xs Additional constructor arguments for `Driver`.
/// @returns The new `stream_manager`, an inbound slot, and an outbound slot.
template <class Driver, class... Ts>
make_sink_result<typename Driver::input_type>
attach_stream_sink(scheduled_actor* self,
                   stream<typename Driver::input_type> in, Ts&&... xs) {
  auto mgr = detail::make_stream_sink<Driver>(self, std::forward<Ts>(xs)...);
  auto slot = mgr->add_inbound_path(in);
  return {slot, std::move(mgr)};
}

/// Attaches a new stream sink to `self` by creating a default stream sink
/// manager from given callbacks.
/// @param self Points to the hosting actor.
/// @param in Stream handshake from upstream path.
/// @param init Function object for initializing the state of the sink.
/// @param fun Processing function.
/// @param fin Optional cleanup handler.
/// @returns The new `stream_manager` and the inbound slot.
template <class In, class Init, class Fun, class Finalize = unit_t,
          class Trait = stream_sink_trait_t<Fun>>
make_sink_result<In> attach_stream_sink(scheduled_actor* self, stream<In> in,
                                        Init init, Fun fun, Finalize fin = {}) {
  using driver = detail::stream_sink_driver_impl<In, Fun, Finalize>;
  return attach_stream_sink<driver>(self, in, std::move(init), std::move(fun),
                                    std::move(fin));
}

} // namespace caf
