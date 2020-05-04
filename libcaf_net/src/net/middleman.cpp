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
#include "caf/detail/set_thread_name.hpp"
#include "caf/init_global_meta_objects.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/net/middleman_backend.hpp"
#include "caf/net/multiplexer.hpp"
#include "caf/raise_error.hpp"
#include "caf/send.hpp"
#include "caf/uri.hpp"

namespace caf::net {

void middleman::init_global_meta_objects() {
  caf::init_global_meta_objects<id_block::net_module>();
}

middleman::middleman(actor_system& sys) : sys_(sys) {
  mpx_ = std::make_shared<multiplexer>();
}

middleman::~middleman() {
  // nop
}

void middleman::start() {
  if (!get_or(config(), "middleman.manual-multiplexing", false)) {
    auto mpx = mpx_;
    auto sys_ptr = &system();
    mpx_thread_ = std::thread{[mpx, sys_ptr] {
      CAF_SET_LOGGER_SYS(sys_ptr);
      detail::set_thread_name("caf.multiplexer");
      sys_ptr->thread_started();
      mpx->set_thread_id();
      mpx->run();
      sys_ptr->thread_terminates();
    }};
  }
}

void middleman::stop() {
  for (const auto& backend : backends_)
    backend->stop();
  mpx_->shutdown();
  if (mpx_thread_.joinable())
    mpx_thread_.join();
  else
    mpx_->run();
}

void middleman::init(actor_system_config& cfg) {
  if (auto err = mpx_->init()) {
    CAF_LOG_ERROR("mgr->init() failed: " << err);
    CAF_RAISE_ERROR("mpx->init() failed");
  }
  if (auto node_uri = get_if<uri>(&cfg, "middleman.this-node")) {
    auto this_node = make_node_id(std::move(*node_uri));
    sys_.node_.swap(this_node);
  } else {
    CAF_RAISE_ERROR("no valid entry for middleman.this-node found");
  }
  for (auto& backend : backends_)
    if (auto err = backend->init()) {
      CAF_LOG_ERROR("failed to initialize backend: " << err);
      CAF_RAISE_ERROR("failed to initialize backend");
    }
}

middleman::module::id_t middleman::id() const {
  return module::network_manager;
}

void* middleman::subtype_ptr() {
  return this;
}

void middleman::resolve(const uri& locator, const actor& listener) {
  auto ptr = backend(locator.scheme());
  if (ptr != nullptr)
    ptr->resolve(locator, listener);
  else
    anon_send(listener, error{basp::ec::invalid_scheme});
}

middleman_backend* middleman::backend(string_view scheme) const noexcept {
  auto predicate = [&](const middleman_backend_ptr& ptr) {
    return ptr->id() == scheme;
  };
  auto i = std::find_if(backends_.begin(), backends_.end(), predicate);
  if (i != backends_.end())
    return i->get();
  return nullptr;
}

} // namespace caf::net
