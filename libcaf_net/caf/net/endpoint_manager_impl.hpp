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

#include "caf/net/endpoint_manager.hpp"

namespace caf {
namespace net {

template <class Transport>
class endpoint_manager_impl : public endpoint_manager {
public:
  // -- member types -----------------------------------------------------------

  using super = endpoint_manager;

  using transport_type = Transport;

  using application_type = typename transport_type::application_type;

  // -- constructors, destructors, and assignment operators --------------------

  endpoint_manager_impl(const multiplexer_ptr& parent, actor_system& sys,
                        Transport trans)
    : super(trans.handle(), parent, sys), transport_(std::move(trans)) {
    // nop
  }

  ~endpoint_manager_impl() override {
    // nop
  }

  // -- properties -------------------------------------------------------------

  transport_type& transport() {
    return transport_;
  }

  // -- interface functions ----------------------------------------------------

  error init() override {
    return transport_.init(*this);
  }

  bool handle_read_event() override {
    return transport_.handle_read_event(*this);
  }

  bool handle_write_event() override {
    if (!this->events_.blocked() && !this->events_.empty()) {
      do {
        this->events_.fetch_more();
        auto& q = this->events_.queue();
        q.inc_deficit(q.total_task_size());
        for (auto ptr = q.next(); ptr != nullptr; ptr = q.next()) {
          using timeout = endpoint_manager::event::timeout;
          using resolve_request = endpoint_manager::event::resolve_request;
          if (auto rr = get_if<resolve_request>(&ptr->value)) {
            transport_.resolve(*this, std::move(rr->path),
                               std::move(rr->listener));
          } else {
            auto& t = get<timeout>(ptr->value);
            transport_.timeout(*this, t.type, t.id);
          }
        }
      } while (!this->events_.try_block());
    }
    return transport_.handle_write_event(*this);
  }

  void handle_error(sec code) override {
    transport_.handle_error(code);
  }

  serialize_fun_type serialize_fun() const noexcept override {
    return application_type::serialize;
  }

private:
  transport_type transport_;
};

} // namespace net
} // namespace caf
