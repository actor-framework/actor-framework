// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/logger.hpp"
#include "caf/net/endpoint_manager_queue.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/packet_writer_decorator.hpp"
#include "caf/unit.hpp"

namespace caf::net {

/// Implements a worker for transport protocols.
template <class Application, class IdType>
class transport_worker {
public:
  // -- member types -----------------------------------------------------------

  using id_type = IdType;

  using application_type = Application;

  // -- constructors, destructors, and assignment operators --------------------

  explicit transport_worker(application_type application,
                            id_type id = id_type{})
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
    auto writer = make_packet_writer_decorator(*this, parent);
    return application_.init(writer);
  }

  template <class Parent>
  error handle_data(Parent& parent, span<const byte> data) {
    auto writer = make_packet_writer_decorator(*this, parent);
    return application_.handle_data(writer, data);
  }

  template <class Parent>
  void write_message(Parent& parent,
                     std::unique_ptr<endpoint_manager_queue::message> msg) {
    auto writer = make_packet_writer_decorator(*this, parent);
    if (auto err = application_.write_message(writer, std::move(msg)))
      CAF_LOG_ERROR("write_message failed: " << err);
  }

  template <class Parent>
  void resolve(Parent& parent, string_view path, const actor& listener) {
    auto writer = make_packet_writer_decorator(*this, parent);
    application_.resolve(writer, path, listener);
  }

  template <class Parent>
  void new_proxy(Parent& parent, const node_id&, actor_id id) {
    auto writer = make_packet_writer_decorator(*this, parent);
    application_.new_proxy(writer, id);
  }

  template <class Parent>
  void
  local_actor_down(Parent& parent, const node_id&, actor_id id, error reason) {
    auto writer = make_packet_writer_decorator(*this, parent);
    application_.local_actor_down(writer, id, std::move(reason));
  }

  template <class Parent>
  void timeout(Parent& parent, std::string tag, uint64_t id) {
    auto writer = make_packet_writer_decorator(*this, parent);
    application_.timeout(writer, std::move(tag), id);
  }

  void handle_error(sec error) {
    application_.handle_error(error);
  }

private:
  application_type application_;
  id_type id_;
};

template <class Application, class IdType = unit_t>
using transport_worker_ptr
  = std::shared_ptr<transport_worker<Application, IdType>>;

} // namespace caf::net
