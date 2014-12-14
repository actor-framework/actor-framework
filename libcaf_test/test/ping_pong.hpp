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

#ifndef PING_PONG_HPP
#define PING_PONG_HPP

//#include "caf/actor.hpp"

#include <cstddef>
#include "caf/fwd.hpp"

void ping(caf::blocking_actor*, size_t num_pings);

void event_based_ping(caf::event_based_actor*, size_t num_pings);

void pong(caf::blocking_actor*, caf::actor ping_actor);

// returns the number of messages ping received
size_t pongs();

#endif // PING_PONG_HPP
