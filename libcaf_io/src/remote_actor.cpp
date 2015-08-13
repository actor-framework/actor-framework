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

actor_addr remote_actor_impl(std::set<std::string> ifs,
                             std::string host, uint16_t port) {
  auto mm = get_middleman_actor();
  actor_addr result;
  scoped_actor self;
  self->sync_send(mm, connect_atom::value, std::move(host), port).await(
    [&](ok_atom, const node_id&, actor_addr res, std::set<std::string>& xs) {
      if (!res)
        throw network_error("no actor published at given port");
      if (! (xs.empty() && ifs.empty())
          && ! std::includes(xs.begin(), xs.end(), ifs.begin(), ifs.end()))
        throw network_error("expected signature does not "
                            "comply to found signature");
      result = std::move(res);
    },
    [&](error_atom, std::string& msg) {
      throw network_error(std::move(msg));
    }
  );
  return result;
}

} // namespace io
} // namespace caf
