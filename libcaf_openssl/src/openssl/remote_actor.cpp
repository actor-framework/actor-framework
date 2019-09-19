/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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
#include "caf/function_view.hpp"

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
  CAF_LOG_TRACE(CAF_ARG(mpi) << CAF_ARG(host) << CAF_ARG(port));
  expected<strong_actor_ptr> res{strong_actor_ptr{nullptr}};
  auto f = make_function_view(sys.openssl_manager().actor_handle());
  auto x = f(connect_atom::value, std::move(host), port);
  if (!x)
    return std::move(x.error());
  auto& tup = *x;
  auto& ptr = get<1>(tup);
  if (!ptr)
    return sec::no_actor_published_at_port;
  auto& found_mpi = get<2>(tup);
  if (sys.assignable(found_mpi, mpi))
    return std::move(ptr);
  return sec::unexpected_actor_messaging_interface;
}

} // namespace openssl
} // namespace caf

