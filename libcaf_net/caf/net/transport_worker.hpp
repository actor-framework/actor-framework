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

#include "caf/byte.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/write_packet_decorator.hpp"
#include "caf/span.hpp"
#include "caf/unit.hpp"

namespace caf {
namespace net {

/// Implements a worker for transport protocols.
template <class Application, class IdType>
class transport_worker {
public:
  // -- member types -----------------------------------------------------------

  using id_type = IdType;

  using application_type = Application;

  // -- constructors, destructors, and assignment operators --------------------

  transport_worker(application_type application, id_type id = id_type{})
    : application_(std::move(application)), id_(std::move(id)) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  application_type& application() noexcept {
    return application_;
  }

  const application_type& application() const noexcept {
    return application_;
  }

  const id_type& id() const noexcept {
    return id_;
  }

  // -- member functions -------------------------------------------------------

  template <class Parent>
  error init(Parent& parent) {
    auto decorator = make_write_packet_decorator(*this, parent);
    return application_.init(decorator);
  }

  template <class Parent>
  error handle_data(Parent& parent, span<const byte> data) {
    auto decorator = make_write_packet_decorator(*this, parent);
    return application_.handle_data(decorator, data);
  }

  template <class Parent>
  void write_message(Parent& parent,
                     std::unique_ptr<net::endpoint_manager::message> msg) {
    auto decorator = make_write_packet_decorator(*this, parent);
    application_.write_message(decorator, std::move(msg));
  }

  template <class Parent>
  void resolve(Parent& parent, string_view path, const actor& listener) {
    auto decorator = make_write_packet_decorator(*this, parent);
    application_.resolve(decorator, path, listener);
  }

  template <class Parent>
  void new_proxy(Parent& parent, const node_id&, actor_id id) {
    auto decorator = make_write_packet_decorator(*this, parent);
    application_.new_proxy(decorator, id);
  }

  template <class Parent>
  void local_actor_down(Parent& parent, const node_id&, actor_id id,
                        error reason) {
    auto decorator = make_write_packet_decorator(*this, parent);
    application_.local_actor_down(decorator, id, std::move(reason));
  }

  template <class Parent>
  void timeout(Parent& parent, atom_value value, uint64_t id) {
    auto decorator = make_write_packet_decorator(*this, parent);
    application_.timeout(decorator, value, id);
  }

  void handle_error(sec error) {
    application_.handle_error(error);
  }

private:
  application_type application_;
  id_type id_;
};

template <class Application, class IdType = unit_t>
using transport_worker_ptr = std::shared_ptr<
  transport_worker<Application, IdType>>;

} // namespace net
} // namespace caf
