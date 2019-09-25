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

#include "caf/net/middleman.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/raise_error.hpp"
#include "caf/uri.hpp"

namespace caf {
namespace net {

middleman::middleman(actor_system& sys) : sys_(sys) {
  mpx_ = std::make_shared<multiplexer>();
}

middleman::~middleman() {
  // nop
}

void middleman::start() {
  if (!get_or(config(), "middleman.manual-multiplexing", false)) {
    auto mpx = mpx_;
    mpx_thread_ = std::thread{[mpx] { mpx->run(); }};
  }
}

void middleman::stop() {
  mpx_->close_pipe();
  if (mpx_thread_.joinable())
    mpx_thread_.join();
}

void middleman::init(actor_system_config&cfg) {
  if (auto err = mpx_->init())
    CAF_RAISE_ERROR("mpx->init failed");
  if (auto node_uri = get_if<uri>(&cfg, "middleman.this-node")) {
    auto this_node = make_node_id(std::move(*node_uri));
    sys_.node_.swap(this_node);
  } else {
    CAF_RAISE_ERROR("no valid entry for middleman.this-node found");
  }
}

actor_system::module::id_t middleman::id() const {
  return module::network_manager;
}

void* middleman::subtype_ptr() {
  return this;
}

actor_system::module* middleman::make(actor_system& sys, detail::type_list<>) {
  return new middleman(sys);
}

} // namespace net
} // namespace caf
