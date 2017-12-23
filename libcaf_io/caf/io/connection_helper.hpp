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

#ifndef CAF_IO_CONNECTION_HELPER_HPP
#define CAF_IO_CONNECTION_HELPER_HPP

#include <chrono>

#include "caf/stateful_actor.hpp"

#include "caf/io/network/interfaces.hpp"

#include "caf/after.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/actor_system_config.hpp"

#include "caf/io/broker.hpp"
#include "caf/io/middleman.hpp"
#include "caf/io/basp_broker.hpp"
#include "caf/io/system_messages.hpp"
#include "caf/io/datagram_handle.hpp"

#include "caf/io/basp/all.hpp"

#include "caf/io/network/datagram_manager.hpp"
#include "caf/io/network/default_multiplexer.hpp"

namespace caf {
namespace io {

struct connection_helper_state {
  static const char* name;
};

behavior datagram_connection_broker(broker* self,
                                    uint16_t port,
                                    network::address_listing addresses,
                                    actor system_broker);

behavior connection_helper(stateful_actor<connection_helper_state>* self,
                           actor b);
} // namespace io
} // namespace caf

#endif // CAF_IO_CONNECTION_HELPER_HPP
