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

#include <stdexcept>

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/net/host.hpp"

namespace {

struct CAF_NET_EXPORT host_fixture {
  host_fixture() {
    if (auto err = caf::net::this_host::startup())
      throw std::logic_error("this_host::startup failed");
  }

  ~host_fixture() {
    caf::net::this_host::cleanup();
  }
};

} // namespace
