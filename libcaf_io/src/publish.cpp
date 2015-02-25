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

#include "caf/io/publish.hpp"

#include "caf/send.hpp"
#include "caf/actor_cast.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/abstract_actor.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/actor_registry.hpp"

#include "caf/io/basp_broker.hpp"
#include "caf/io/middleman_actor.hpp"

namespace caf {
namespace io {

uint16_t publish_impl(abstract_actor_ptr whom, uint16_t port,
                      const char* in, bool reuse) {
  if (whom == nullptr) {
    throw std::invalid_argument("cannot publish an invalid actor");
  }
  CAF_LOGF_TRACE("whom = " << to_string(whom->address())
                 << ", " << CAF_ARG(port) << ", in = " << (in ? in : "")
                 << ", " << CAF_ARG(reuse));
  std::string str;
  if (in != nullptr) {
    str = in;
  }
  auto mm = get_middleman_actor();
  scoped_actor self;
  uint16_t result;
  self->sync_send(mm, put_atom::value, whom->address(), port, str, reuse).await(
    [&](ok_atom, uint16_t res) {
      result = res;
    },
    [&](error_atom, std::string& msg) {
      throw network_error(std::move(msg));
    }
  );
  return result;
}

} // namespace io
} // namespace caf
