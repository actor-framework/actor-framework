/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#include "caf/openssl/remote_actor.hpp"

#include "caf/sec.hpp"
#include "caf/atom.hpp"
#include "caf/expected.hpp"
#include "caf/scoped_actor.hpp"

#include "caf/openssl/manager.hpp"

namespace caf {
namespace openssl {

/// Establish a new connection to the actor at `host` on given `port`.
/// @param host Valid hostname or IP address.
/// @param port TCP port.
/// @returns An `actor` to the proxy instance representing
///          a remote actor or an `error`.
expected<strong_actor_ptr> remote_actor(actor_system& sys,
                                        const std::set<std::string>& mpi,
                                        std::string host, uint16_t port) {
  CAF_LOG_TRACE(CAF_ARG(expected) << CAF_ARG(host) << CAF_ARG(port));
  expected<strong_actor_ptr> res{strong_actor_ptr{nullptr}};
  scoped_actor self{sys};
  self->request(sys.openssl_manager().actor_handle(), infinite,
                connect_atom::value, std::move(host), port)
  .receive(
    [&](const node_id&, strong_actor_ptr& ptr, std::set<std::string>& found) {
      if (res) {
        if (sys.assignable(found, mpi))
          res = std::move(ptr);
        else
          res = sec::unexpected_actor_messaging_interface;
      }
      res = sec::no_actor_published_at_port;
    },
    [&](error& err) {
      res = std::move(err);
    }
  );
  return res;
}

} // namespace openssl
} // namespace caf

