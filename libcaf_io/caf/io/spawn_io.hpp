/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_SPAWN_IO_HPP
#define CAF_IO_SPAWN_IO_HPP

#include <tuple>
#include <functional>

#include "caf/spawn.hpp"
#include "caf/mailbox_element.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/abstract_broker.hpp"
#include "caf/io/connection_handle.hpp"
#include "caf/io/experimental/typed_broker.hpp"

#include "caf/io/network/native_socket.hpp"

namespace caf {
namespace io {

/// @cond PRIVATE

template <spawn_options Os, class Impl, class F, class... Ts>
intrusive_ptr<Impl> spawn_io_client_impl(F fun, const std::string& host,
                                         uint16_t port, Ts&&... xs) {
  // works around an issue with GCC 4.8 that could not handle
  // variadic template parameter packs inside lambdas
  auto args = std::forward_as_tuple(std::forward<Ts>(xs)...);
  auto bl = [&](Impl* ptr) {
    auto mm = middleman::instance();
    auto hdl = mm->backend().add_tcp_scribe(ptr, host, port);
    detail::init_fun_factory<Impl, F> fac;
    auto init = detail::apply_args_prefixed(fac, detail::get_indices(args),
                                            args, std::move(fun), hdl);
    ptr->initial_behavior_fac(std::move(init));
  };
  return spawn_class<Impl>(nullptr, bl);
}

template <spawn_options Os, class Impl, class F, class... Ts>
intrusive_ptr<Impl> spawn_io_server_impl(F fun, uint16_t port, Ts&&... xs) {
  detail::init_fun_factory<Impl, F> fac;
  auto init = fac(std::move(fun), std::forward<Ts>(xs)...);
  auto bl = [&](Impl* ptr) {
    auto mm = middleman::instance();
    mm->backend().add_tcp_doorman(ptr, port);
    ptr->initial_behavior_fac(std::move(init));
  };
  return spawn_class<Impl>(nullptr, bl);
}

/// @endcond

/// Spawns a new functor-based broker.
template <spawn_options Os = no_spawn_options,
          class F = std::function<void(broker*)>, class... Ts>
actor spawn_io(F fun, Ts&&... xs) {
  return spawn_functor<Os>(nullptr, empty_before_launch_callback{},
                           std::move(fun), std::forward<Ts>(xs)...);
}

/// Spawns a new functor-based broker connecting to `host:port`.
template <spawn_options Os = no_spawn_options,
          class F = std::function<void(broker*)>, class... Ts>
actor spawn_io_client(F fun, const std::string& host,
                      uint16_t port, Ts&&... xs) {
  return spawn_io_client_impl<Os, broker>(std::move(fun), host, port,
                                          std::forward<Ts>(xs)...);
}

/// Spawns a new broker as server running on given `port`.
template <spawn_options Os = no_spawn_options,
          class F = std::function<void(broker*)>, class... Ts>
actor spawn_io_server(F fun, uint16_t port, Ts&&... xs) {
  return spawn_io_server_impl<Os, broker>(std::move(fun), port,
                                          std::forward<Ts>(xs)...);
}

namespace experimental {

/// Spawns a new functor-based typed-broker.
template <spawn_options Os = no_spawn_options, class F, class... Ts>
typename infer_typed_actor_handle<
  typename detail::get_callable_trait<F>::result_type,
  typename detail::tl_head<
    typename detail::get_callable_trait<F>::arg_types
  >::type
>::type
spawn_io_typed(F fun, Ts&&... xs) {
  using impl =
    typename infer_typed_broker_base<
      typename detail::get_callable_trait<F>::result_type,
      typename detail::tl_head<
        typename detail::get_callable_trait<F>::arg_types
      >::type
    >::type;
  return spawn_functor_impl<Os, impl>(nullptr,
                                      empty_before_launch_callback{},
                                      std::move(fun), std::forward<Ts>(xs)...);
}

/// Spawns a new functor-based typed-broker connecting to `host:port`.
template <spawn_options Os = no_spawn_options, class F, class... Ts>
typename infer_typed_actor_handle<
  typename detail::get_callable_trait<F>::result_type,
  typename detail::tl_head<
    typename detail::get_callable_trait<F>::arg_types
  >::type
>::type
spawn_io_client_typed(F fun, const std::string& host, uint16_t port,
                      Ts&&... xs) {
  using trait = typename detail::get_callable_trait<F>::type;
  using arg_types = typename trait::arg_types;
  using first_arg = typename detail::tl_head<arg_types>::type;
  using impl_class = typename std::remove_pointer<first_arg>::type;
  static_assert(std::is_convertible<typename impl_class::actor_hdl,
                                    minimal_client>::value,
                "Cannot spawn io client: broker misses required handlers");
  return spawn_io_client_impl<Os, impl_class>(std::move(fun), host, port,
                                              std::forward<Ts>(xs)...);
}

/// Spawns a new typed-broker as server running on given `port`.
template <spawn_options Os = no_spawn_options, class F, class... Ts>
typename infer_typed_actor_handle<
  typename detail::get_callable_trait<F>::result_type,
  typename detail::tl_head<
    typename detail::get_callable_trait<F>::arg_types
  >::type
>::type
spawn_io_server_typed(F fun, uint16_t port, Ts&&... xs) {
  using trait = typename detail::get_callable_trait<F>::type;
  using arg_types = typename trait::arg_types;
  using first_arg = typename detail::tl_head<arg_types>::type;
  using impl_class = typename std::remove_pointer<first_arg>::type;
  static_assert(std::is_convertible<typename impl_class::actor_hdl,
                                    minimal_server>::value,
                "Cannot spawn io server: broker misses required handlers");
  return spawn_io_server_impl<Os, impl_class>(std::move(fun), port,
                                              std::forward<Ts>(xs)...);
}

} // namespace experimental


} // namespace io
} // namespace caf

#endif // CAF_IO_SPAWN_IO_HPP
