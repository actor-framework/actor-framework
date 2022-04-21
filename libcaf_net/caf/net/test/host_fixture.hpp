// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <stdexcept>

#include "caf/detail/net_export.hpp"
#include "caf/error.hpp"
#include "caf/net/host.hpp"
#include "caf/raise_error.hpp"

namespace {

struct CAF_NET_EXPORT host_fixture {
  host_fixture() {
    if (auto err = caf::net::this_host::startup())
      CAF_RAISE_ERROR(std::logic_error, "this_host::startup failed");
  }

  ~host_fixture() {
    caf::net::this_host::cleanup();
  }
};

} // namespace
