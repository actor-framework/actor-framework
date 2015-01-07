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

#include "caf/io/remote_actor.hpp"

#include <set>
#include <ios>
#include <string>
#include <vector>
#include <future>
#include <cstdint>
#include <algorithm>

#include "caf/send.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/network/multiplexer.hpp"

#include "caf/detail/logging.hpp"

namespace caf {
namespace io {

abstract_actor_ptr remote_actor_impl(std::set<std::string> ifs,
                                     const std::string& host, uint16_t port) {
  auto mm = middleman::instance();
  scoped_actor self;
  actor selfhdl = self;
  basp_broker::client_handshake_data hdata{invalid_node_id,
                                           selfhdl, std::move(ifs)};
  mm->run_later([&] {
    std::string err;
    try {
      auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
      auto hdl = mm->backend().add_tcp_scribe(bro.get(), host, port);
      bro->init_client(hdl, &hdata);
    }
    catch (std::exception& e) {
      err = e.what();
    }
    // accessing variables from the outer scope inside the
    // catch block triggers a silly compiler error on GCC 4.7
    if (!err.empty()) {
      anon_send(selfhdl, atom("ERROR"), std::move(err));
    }
  });
  abstract_actor_ptr result;
  self->receive(
    on(atom("OK"), arg_match) >> [&](const actor& res) {
      result = actor_cast<abstract_actor_ptr>(res);
    },
    on(atom("ERROR"), arg_match) >> [](std::string& str) {
      throw network_error(std::move(str));
    }
  );
  return result;
}

} // namespace io
} // namespace caf

