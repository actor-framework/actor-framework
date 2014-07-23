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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_PUBLISH_IMPL_HPP
#define CAF_IO_PUBLISH_IMPL_HPP

#include "caf/abstract_actor.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

#include "caf/io/network.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"

namespace caf {
namespace io {

template <class ActorHandle, class SocketAcceptor>
void publish_impl(ActorHandle whom, SocketAcceptor fd) {
  using namespace detail;
  auto mm = middleman::instance();
  // we can't move fd into our lambda in C++11 ...
  using pair_type = std::pair<ActorHandle, SocketAcceptor>;
  auto data = std::make_shared<pair_type>(std::move(whom), std::move(fd));
  mm->run_later([mm, data] {
    auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
    bro->publish(std::move(data->first), std::move(data->second));
  });
}

inline void publish_impl(abstract_actor_ptr whom, uint16_t port,
             const char* ipaddr) {
  using namespace detail;
  auto mm = middleman::instance();
  auto addr = whom->address();
  network::default_socket_acceptor fd{mm->backend()};
  network::ipv4_bind(fd, port, ipaddr);
  publish_impl(std::move(whom), std::move(fd));
}

} // namespace io
} // namespace caf

#endif // CAF_IO_PUBLISH_IMPL_HPP
