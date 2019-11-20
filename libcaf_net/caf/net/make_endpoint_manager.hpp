/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/make_counted.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/endpoint_manager_impl.hpp"

namespace caf::net {

template <class Transport>
endpoint_manager_ptr make_endpoint_manager(const multiplexer_ptr& mpx,
                                           actor_system& sys, Transport trans) {
  using impl = endpoint_manager_impl<Transport>;
  return make_counted<impl>(mpx, sys, std::move(trans));
}

} // namespace caf::net
