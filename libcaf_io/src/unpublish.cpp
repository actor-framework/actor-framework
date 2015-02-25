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

#include "caf/io/unpublish.hpp"

#include "caf/send.hpp"
#include "caf/scoped_actor.hpp"

#include "caf/io/unpublish.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/middleman_actor.hpp"

namespace caf {
namespace io {

void unpublish_impl(const actor_addr& whom, uint16_t port, bool blocking) {
  CAF_LOGF_TRACE(CAF_TSARG(whom) << ", " << CAF_ARG(port) << CAF_ARG(blocking));
  auto mm = get_middleman_actor();
  if (blocking) {
    scoped_actor self;
    self->sync_send(mm, delete_atom::value, whom, port).await(
      [](ok_atom) {
        // ok, basp_broker is done
      },
      [](error_atom, const std::string&) {
        // ok, basp_broker is done
      }
    );
  } else {
    anon_send(mm, delete_atom::value, whom, port);
  }
}

} // namespace io
} // namespace caf
