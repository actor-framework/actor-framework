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

#include <unordered_map>

#include "caf/byte.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/net/endpoint_manager.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/transport_worker.hpp"
#include "caf/net/write_packet_decorator.hpp"
#include "caf/span.hpp"
#include "caf/unit.hpp"

namespace caf {
namespace net {

/// implements a dispatcher that dispatches calls to the right worker
template <class ApplicationFactory, class IdType>
class transport_worker_dispatcher {
public:
  // -- member types -----------------------------------------------------------

  using id_type = IdType;

  using factory_type = ApplicationFactory;

  using application_type = typename ApplicationFactory::application_type;

  using worker_type = transport_worker<application_type, id_type>;

  using worker_ptr = transport_worker_ptr<application_type, id_type>;

  // -- constructors, destructors, and assignment operators --------------------

  transport_worker_dispatcher(factory_type factory)
    : factory_(std::move(factory)) {
    // nop
  }

  // -- member functions -------------------------------------------------------

  /// Initializes all contained contained workers.
  /// @param The parent of this Dispatcher
  /// @return error if something went wrong sec::none when init() worked.
  template <class Parent>
  error init(Parent& parent) {
    for (auto worker : workers_by_id_) {
      if (auto err = worker->init(parent))
        return err;
    }
    return none;
  }

  template <class Parent>
  void handle_data(Parent& parent, span<byte> data, id_type id) {
    auto it = workers_by_id_.find(id);
    if (it == workers_by_id_.end()) {
      // TODO: where to get node_id from in this scope.
      add_new_worker(node_id{}, id);
      it = workers_by_id_.find(id);
    }
    auto worker = it->second;
    worker->handle_data(parent, data);
  }

  template <class Parent>
  void write_message(Parent& parent,
                     std::unique_ptr<net::endpoint_manager::message> msg) {
    auto sender = msg->msg->sender;
    if (!sender)
      return;
    auto nid = sender->node();
    auto it = workers_by_node_.find(nid);
    if (it == workers_by_node_.end()) {
      // TODO: where to get id_type from in this scope.
      add_new_worker(nid, id_type{});
      it = workers_by_node_.find(nid);
    }
    auto worker = it->second;
    // parent should be a decorator with parent and parent->parent.
    worker->write_message(parent, std::move(msg));
  }

  template <class Parent>
  void write_packet(Parent& parent, span<const byte> header,
                    span<const byte> payload, id_type id) {
    parent.write_packet(header, payload, id);
  }

  template <class Parent>
  void resolve(Parent& parent, const std::string& path, actor listener) {
    // TODO: How to find the right worker? use path somehow?
    if (workers_by_id_.empty())
      return;
    auto worker = workers_by_id_.begin()->second;
    worker->resolve(parent, path, listener);
  }

  template <class... Ts>
  void set_timeout(uint64_t timeout_id, id_type id, Ts&&...) {
    workers_by_timeout_id_.emplace(timeout_id, workers_by_id_.at(id));
  }

  template <class Parent>
  void timeout(Parent& parent, atom_value value, uint64_t id) {
    auto worker = workers_by_timeout_id_.at(id);
    worker->application_.timeout(parent, value, id);
    workers_by_timeout_id_.erase(id);
  }

  void handle_error(sec error) {
    for (auto worker : workers_by_id_) {
      if (auto err = worker->handle_error(error))
        return err;
    }
  }

  void add_new_worker(node_id node, id_type id) {
    auto application = factory_.make();
    auto worker = std::make_shared<worker_type>(std::move(application), id);
    workers_by_id_.emplace(std::move(id), worker);
    workers_by_node_.emplace(std::move(node), std::move(worker));
  }

private:
  // -- worker lookups ---------------------------------------------------------

  std::unordered_map<id_type, worker_ptr> workers_by_id_;
  std::unordered_map<node_id, worker_ptr> workers_by_node_;
  std::unordered_map<uint64_t, worker_ptr> workers_by_timeout_id_;

  /// Factory to make application instances for transport_workers
  factory_type factory_;
}; // namespace net

} // namespace net
} // namespace caf
