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

#ifndef CAF_IO_REMOTE_ACTOR_IMPL_HPP
#define CAF_IO_REMOTE_ACTOR_IMPL_HPP

#include <set>
#include <ios>
#include <string>
#include <vector>
#include <future>
#include <cstdint>
#include <algorithm>

#include "caf/abstract_actor.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/io/network.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"

#include "caf/detail/logging.hpp"

namespace caf {
namespace io {

constexpr uint32_t max_iface_size = 100;

constexpr uint32_t max_iface_clause_size = 500;

template <class Socket>
abstract_actor_ptr remote_actor_impl(Socket fd,
                   const std::set<std::string>& ifs) {
  auto mm = middleman::instance();
  std::string error_msg;
  std::promise<abstract_actor_ptr> result_promise;
  // we can't move fd into our lambda in C++11 ...
  auto fd_ptr = std::make_shared<Socket>(std::move(fd));
  basp_broker::client_handshake_data hdata{
    invalid_node_id, &result_promise, &error_msg, &ifs};
  auto hdata_ptr = &hdata;
  mm->run_later([=] {
    auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
    auto hdl = bro->add_connection(std::move(*fd_ptr));
    bro->init_client(hdl, hdata_ptr);
  });
  auto result = result_promise.get_future().get();
  if (!result) throw std::runtime_error(error_msg);
  return result;
}

inline abstract_actor_ptr
remote_actor_impl(const std::string& host, uint16_t port,
          const std::set<std::string>& ifs) {
  auto mm = middleman::instance();
  network::default_socket fd{mm->backend()};
  network::ipv4_connect(fd, host, port);
  return remote_actor_impl(std::move(fd), ifs);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_REMOTE_ACTOR_IMPL_HPP
