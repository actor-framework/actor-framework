// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
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
