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

#include <tuple>

#include "caf/broadcast_downstream_manager.hpp"
#include "caf/default_downstream_manager.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/stream_source_driver_impl.hpp"
#include "caf/detail/stream_source_impl.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/downstream_manager.hpp"
#include "caf/fwd.hpp"
#include "caf/is_actor_handle.hpp"
#include "caf/make_source_result.hpp"
#include "caf/policy/arg.hpp"
#include "caf/response_type.hpp"
#include "caf/stream_source.hpp"

namespace caf {

/// Attaches a new stream source to `self` by creating a default stream source
/// manager with `Driver`.
/// @param self Points to the hosting actor.
/// @param xs User-defined arguments for the stream handshake.
/// @param ctor_args Parameter pack for constructing the driver.
/// @returns The allocated `stream_manager` and the output slot.
template <class Driver, class... Ts, class... CtorArgs>
make_source_result_t<typename Driver::downstream_manager_type, Ts...>
attach_stream_source(scheduled_actor* self, std::tuple<Ts...> xs,
                     CtorArgs&&... ctor_args) {
  using namespace detail;
  auto mgr = make_stream_source<Driver>(self,
                                        std::forward<CtorArgs>(ctor_args)...);
  auto slot = mgr->add_outbound_path(std::move(xs));
  return {slot, std::move(mgr)};
}

/// Attaches a new stream source to `self` by creating a default stream source
/// manager with the default driver.
/// @param self Points to the hosting actor.
/// @param xs User-defined arguments for the stream handshake.
/// @param init Function object for initializing the state of the source.
/// @param pull Generator function object for producing downstream messages.
/// @param done Predicate returning `true` when generator is done.
/// @param fin Cleanup handler.
/// @returns The allocated `stream_manager` and the output slot.
template <class... Ts, class Init, class Pull, class Done,
          class Finalize = unit_t, class Trait = stream_source_trait_t<Pull>,
          class DownstreamManager = broadcast_downstream_manager<
            typename Trait::output>>
make_source_result_t<DownstreamManager, Ts...>
attach_stream_source(scheduled_actor* self, std::tuple<Ts...> xs, Init init,
                     Pull pull, Done done, Finalize fin = {},
                     policy::arg<DownstreamManager> = {}) {
  using state_type = typename Trait::state;
  static_assert(std::is_same<
                  void(state_type&),
                  typename detail::get_callable_trait<Init>::fun_sig>::value,
                "Expected signature `void (State&)` for init function");
  static_assert(std::is_same<
                  bool(const state_type&),
                  typename detail::get_callable_trait<Done>::fun_sig>::value,
                "Expected signature `bool (const State&)` "
                "for done predicate function");
  using driver = detail::stream_source_driver_impl<DownstreamManager, Pull,
                                                   Done, Finalize>;
  return attach_stream_source<driver>(self, std::move(xs), std::move(init),
                                      std::move(pull), std::move(done),
                                      std::move(fin));
}

/// Attaches a new stream source to `self` by creating a default stream source
/// manager with the default driver.
/// @param self Points to the hosting actor.
/// @param init Function object for initializing the state of the source.
/// @param pull Generator function object for producing downstream messages.
/// @param done Predicate returning `true` when generator is done.
/// @param fin Cleanup handler.
/// @returns The allocated `stream_manager` and the output slot.
template <class Init, class Pull, class Done, class Finalize = unit_t,
          class DownstreamManager = default_downstream_manager_t<Pull>,
          class Trait = stream_source_trait_t<Pull>>
detail::enable_if_t<!is_actor_handle<Init>::value && Trait::valid,
                    make_source_result_t<DownstreamManager>>
attach_stream_source(scheduled_actor* self, Init init, Pull pull, Done done,
                     Finalize fin = {},
                     policy::arg<DownstreamManager> token = {}) {
  using output_type = typename Trait::output;
  static_assert(detail::sendable<output_type>,
                "the output type of the source has has no type ID, "
                "did you forgot to announce it via CAF_ADD_TYPE_ID?");
  static_assert(detail::sendable<stream<output_type>>,
                "stream<T> for the output type has has no type ID, "
                "did you forgot to announce it via CAF_ADD_TYPE_ID?");
  return attach_stream_source(self, std::make_tuple(), init, pull, done, fin,
                              token);
}

/// Attaches a new stream source to `self` by creating a default stream source
/// manager with the default driver and starts sending to `dest` immediately.
/// @param self Points to the hosting actor.
/// @param dest Handle to the next stage in the pipeline.
/// @param xs User-defined arguments for the stream handshake.
/// @param init Function object for initializing the state of the source.
/// @param pull Generator function object for producing downstream messages.
/// @param done Predicate returning `true` when generator is done.
/// @param fin Cleanup handler.
/// @returns The allocated `stream_manager` and the output slot.
template <class ActorHandle, class... Ts, class Init, class Pull, class Done,
          class Finalize = unit_t,
          class DownstreamManager = default_downstream_manager_t<Pull>,
          class Trait = stream_source_trait_t<Pull>>
detail::enable_if_t<is_actor_handle<ActorHandle>::value,
                    make_source_result_t<DownstreamManager>>
attach_stream_source(scheduled_actor* self, const ActorHandle& dest,
                     std::tuple<Ts...> xs, Init init, Pull pull, Done done,
                     Finalize fin = {}, policy::arg<DownstreamManager> = {}) {
  using namespace detail;
  using token = type_list<stream<typename DownstreamManager::output_type>,
                          strip_and_convert_t<Ts>...>;
  static_assert(response_type_unbox<signatures_of_t<ActorHandle>, token>::valid,
                "receiver does not accept the stream handshake");
  using driver = detail::stream_source_driver_impl<DownstreamManager, Pull,
                                                   Done, Finalize>;
  auto mgr = detail::make_stream_source<driver>(self, std::move(init),
                                                std::move(pull),
                                                std::move(done),
                                                std::move(fin));
  auto slot = mgr->add_outbound_path(dest, std::move(xs));
  return {slot, std::move(mgr)};
}

/// Attaches a new stream source to `self` by creating a default stream source
/// manager with the default driver and starts sending to `dest` immediately.
/// @param self Points to the hosting actor.
/// @param dest Handle to the next stage in the pipeline.
/// @param init Function object for initializing the state of the source.
/// @param pull Generator function object for producing downstream messages.
/// @param done Predicate returning `true` when generator is done.
/// @param fin Cleanup handler.
/// @returns The allocated `stream_manager` and the output slot.
template <class ActorHandle, class Init, class Pull, class Done,
          class Finalize = unit_t,
          class DownstreamManager = default_downstream_manager_t<Pull>,
          class Trait = stream_source_trait_t<Pull>>
detail::enable_if_t<is_actor_handle<ActorHandle>::value && Trait::valid,
                    make_source_result_t<DownstreamManager>>
attach_stream_source(scheduled_actor* self, const ActorHandle& dest, Init init,
                     Pull pull, Done done, Finalize fin = {},
                     policy::arg<DownstreamManager> token = {}) {
  return attach_stream_source(self, dest, std::make_tuple(), std::move(init),
                              std::move(pull), std::move(done), std::move(fin),
                              token);
}

} // namespace caf
