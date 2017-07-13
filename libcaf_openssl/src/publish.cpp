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

#include "caf/openssl/publish.hpp"

#include <set>

#include "caf/atom.hpp"
#include "caf/expected.hpp"
#include "caf/actor_system.hpp"
#include "caf/function_view.hpp"
#include "caf/actor_control_block.hpp"

#include "caf/openssl/manager.hpp"

namespace caf {
namespace openssl {

expected<uint16_t> publish(actor_system& sys, const strong_actor_ptr& whom,
                           std::set<std::string>&& sigs, uint16_t port,
                           const char* cstr, bool ru) {
  CAF_LOG_TRACE(CAF_ARG(whom) << CAF_ARG(sigs) << CAF_ARG(port));
  CAF_ASSERT(whom != nullptr);
  std::string in;
  if (cstr != nullptr)
    in = cstr;
  auto f = make_function_view(sys.openssl_manager().actor_handle());
  return f(publish_atom::value, port, std::move(whom), std::move(sigs),
           std::move(in), ru);
}

} // namespace openssl
} // namespace caf

