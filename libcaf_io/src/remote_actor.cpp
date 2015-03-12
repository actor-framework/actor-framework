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

#include "caf/io/remote_actor.hpp"

#include <set>
#include <ios>
#include <string>
#include <vector>
#include <future>
#include <cstdint>
#include <algorithm>

#include "caf/send.hpp"
#include "caf/exception.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/binary_deserializer.hpp"

#include "caf/io/basp_broker.hpp"
#include "caf/io/middleman_actor.hpp"
#include "caf/io/network/multiplexer.hpp"

#include "caf/detail/logging.hpp"

namespace caf {
namespace io {

abstract_actor_ptr remote_actor_impl(std::set<std::string> ifs,
                                     const std::string& host, uint16_t port) {
  auto mm = get_middleman_actor();
  scoped_actor self;
  abstract_actor_ptr result;
  self->sync_send(mm, get_atom{}, std::move(host), port, std::move(ifs)).await(
    [&](ok_atom, actor_addr res) {
      result = actor_cast<abstract_actor_ptr>(res);
    },
    [&](error_atom, std::string& msg) {
      throw network_error(std::move(msg));
    }
  );
  return result;
}

} // namespace io
} // namespace caf

