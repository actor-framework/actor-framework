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

#ifndef CAF_IO_PUBLISH_HPP
#define CAF_IO_PUBLISH_HPP

#include <cstdint>

#include "caf/actor.hpp"
#include "caf/actor_cast.hpp"

#include "caf/io/publish_impl.hpp"

namespace caf {
namespace io {

/**
 * Publishes `whom` at `port`. The connection is managed by the middleman.
 * @param whom Actor that should be published at `port`.
 * @param port Unused TCP port.
 * @param addr The IP address to listen to or `INADDR_ANY` if `in == nullptr`.
 * @throws bind_failure
 */
inline void publish(caf::actor whom, uint16_t port, const char* in = nullptr) {
  if (!whom) {
    return;
  }
  io::publish_impl(actor_cast<abstract_actor_ptr>(whom), port, in);
}

/**
 * @copydoc publish(actor,uint16_t,const char*)
 */
template <class... Rs>
void typed_publish(typed_actor<Rs...> whom, uint16_t port,
                   const char* in = nullptr) {
  if (!whom) {
    return;
  }
  io::publish_impl(actor_cast<abstract_actor_ptr>(whom), port, in);
}

} // namespace io
} // namespace caf

#endif // CAF_IO_PUBLISH_HPP
