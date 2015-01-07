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

#include "caf/io/publish.hpp"

#include "caf/send.hpp"
#include "caf/actor_cast.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/abstract_actor.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"

namespace caf {
namespace io {

uint16_t publish_impl(abstract_actor_ptr whom, uint16_t port,
                      const char* in, bool reuse_addr) {
  using namespace detail;
  auto mm = middleman::instance();
  scoped_actor self;
  actor selfhdl = self;
  mm->run_later([&] {
    auto bro = mm->get_named_broker<basp_broker>(atom("_BASP"));
    try {
      auto hdl = mm->backend().add_tcp_doorman(bro.get(), port, in, reuse_addr);
      bro->add_published_actor(std::move(hdl.first), whom, hdl.second);
      mm->notify<hook::actor_published>(whom->address(), hdl.second);
      anon_send(selfhdl, atom("OK"), hdl.second);
    }
    catch (bind_failure& e) {
      anon_send(selfhdl, atom("BIND_FAIL"), e.what());
    }
    catch (network_error& e) {
      anon_send(selfhdl, atom("ERROR"), e.what());
    }
  });
  uint16_t bound_port = 0;
  // block caller and re-throw exception here in case of an error
  self->receive(
    on(atom("OK"), arg_match) >> [&](uint16_t p) {
      bound_port = p;
    },
    on(atom("BIND_FAIL"), arg_match) >> [](std::string& str) {
      throw bind_failure(std::move(str));
    },
    on(atom("ERROR"), arg_match) >> [](std::string& str) {
      throw network_error(std::move(str));
    }
  );
  return bound_port;
}

} // namespace io
} // namespace caf
