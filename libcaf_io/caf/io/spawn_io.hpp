/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#include <functional>

#include "caf/spawn.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/connection_handle.hpp"

#include "caf/io/network/native_socket.hpp"

namespace caf {
namespace io {

/**
 * Spawns a new functor-based broker.
 */
template <spawn_options Os = no_spawn_options,
     typename F = std::function<void(broker*)>, class... Ts>
actor spawn_io(F fun, Ts&&... vs) {
  return spawn<broker::functor_based>(std::move(fun), std::forward<Ts>(vs)...);
}

/**
 * Spawns a new functor-based broker connecting to `host:port.
 */
template <spawn_options Os = no_spawn_options,
      typename F = std::function<void(broker*)>, class... Ts>
actor spawn_io_client(F fun, const std::string& host,
                      uint16_t port, Ts&&... args) {
  // provoke compiler error early
  using fun_res = decltype(fun(static_cast<broker*>(nullptr),
                               connection_handle{},
                               std::forward<Ts>(args)...));
  // prevent warning about unused local type
  static_assert(std::is_same<fun_res, fun_res>::value,
                "your compiler is lying to you");
  // works around an issue with older GCC releases that could not handle
  // variadic template parameter packs inside lambdas by storing into a
  // std::function first (using `auto init =` won't compile on Clang)
  auto mfptr = &broker::functor_based::init<F, connection_handle, Ts...>;
  using bi = std::function<void (broker::functor_based*, F, connection_handle)>;
  using namespace std::placeholders;
  bi init = std::bind(mfptr, _1, _2, _3, std::forward<Ts>(args)...);
  return spawn_class<broker::functor_based>(
        nullptr,
        [&](broker::functor_based* ptr) {
          auto mm = middleman::instance();
          auto hdl = mm->backend().add_tcp_scribe(ptr, host, port);
          init(ptr, fun, hdl);
        });
}

/**
 * Spawns a new broker as server running on given `port`.
 */
template <spawn_options Os = no_spawn_options,
          class F = std::function<void(broker*)>, class... Ts>
actor spawn_io_server(F fun, uint16_t port, Ts&&... args) {
  // same workaround as above
  auto mfptr = &broker::functor_based::init<F, Ts...>;
  using bi = std::function<void (broker::functor_based*, F)>;
  using namespace std::placeholders;
  bi init = std::bind(mfptr, _1, _2, std::forward<Ts>(args)...);
  return spawn_class<broker::functor_based>(
        nullptr,
        [&](broker::functor_based* ptr) {
          auto mm = middleman::instance();
          mm->backend().add_tcp_doorman(ptr, port);
          init(ptr, std::move(fun));
        });
}

} // namespace io
} // namespace caf

#endif // CAF_IO_SPAWN_IO_HPP
