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

#include "caf/broadcast_downstream_manager.hpp"
#include "caf/detail/stream_source_driver_impl.hpp"
#include "caf/detail/stream_source_impl.hpp"
#include "caf/fwd.hpp"
#include "caf/policy/arg.hpp"
#include "caf/stream_source.hpp"
#include "caf/stream_source_driver.hpp"
#include "caf/stream_source_trait.hpp"

namespace caf {

/// Creates a new continuous stream source by instantiating the default
/// source implementation with `Driver. `The returned manager is not
/// connected to any slot and thus not stored by the actor automatically.
/// @param self Points to the hosting actor.
/// @param init Function object for initializing the state of the source.
/// @param pull Generator function object for producing downstream messages.
/// @param done Predicate returning `true` when generator is done.
/// @param fin Cleanup handler.
/// @returns The new `stream_manager`.
template <class Driver, class Init, class Pull, class Done,
          class Finalize = unit_t>
typename Driver::source_ptr_type
attach_continuous_stream_source(scheduled_actor* self, Init init, Pull pull,
                                Done done, Finalize fin = {}) {
  using detail::make_stream_source;
  auto mgr = make_stream_source<Driver>(self, std::move(init), std::move(pull),
                                        std::move(done), std::move(fin));
  mgr->continuous(true);
  return mgr;
}

/// Creates a new continuous stream source by instantiating the default
/// source implementation with `Driver. `The returned manager is not
/// connected to any slot and thus not stored by the actor automatically.
/// @param self Points to the hosting actor.
/// @param init Function object for initializing the state of the source.
/// @param pull Generator function object for producing downstream messages.
/// @param done Predicate returning `true` when generator is done.
/// @param fin Cleanup handler.
/// @returns The new `stream_manager`.
template <class Init, class Pull, class Done, class Finalize = unit_t,
          class DownstreamManager = broadcast_downstream_manager<
            typename stream_source_trait_t<Pull>::output>>
stream_source_ptr<DownstreamManager>
attach_continuous_stream_source(scheduled_actor* self, Init init, Pull pull,
                                Done done, Finalize fin = {},
                                policy::arg<DownstreamManager> = {}) {
  using driver = detail::stream_source_driver_impl<DownstreamManager, Pull,
                                                   Done, Finalize>;
  return attach_continuous_stream_source<driver>(self, std::move(init),
                                                 std::move(pull),
                                                 std::move(done),
                                                 std::move(fin));
}

} // namespace caf
