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

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/multiplexer.hpp"

#include "caf/detail/logging.hpp"

namespace caf {
namespace io {

template <class... Ts>
abstract_actor_ptr remote_actor_impl(const std::set<std::string>& ifs,
                                     const std::string& host, uint16_t port) {
  auto mm = middleman::instance();
  std::string error_msg;
  std::promise<abstract_actor_ptr> result_promise;
  basp_broker::client_handshake_data hdata{invalid_node_id, &result_promise,
                                           &error_msg, &ifs};
  mm->run_later([&] {
    try {
      auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
      auto hdl = mm->backend().add_tcp_scribe(bro.get(), host, port);
      bro->init_client(hdl, &hdata);
    }
    catch (...) {
      result_promise.set_exception(std::current_exception());
    }
  });
  auto result = result_promise.get_future().get();
  if (!result) {
    throw std::runtime_error(error_msg);
  }
  return result;
}

} // namespace io
} // namespace caf

#endif // CAF_IO_REMOTE_ACTOR_IMPL_HPP
