/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <caf/actor_config.hpp>
#include <caf/binary_serializer.hpp>
#include <caf/expected.hpp>
#include <caf/logger.hpp>
#include <caf/make_actor.hpp>
#include <caf/span.hpp>
#include <caf/test/dsl.hpp>
#include <netinet/in.h>
#include <vector>

namespace caf {
namespace policy {

struct string_application_header {
  uint32_t payload;
};

/// @relates header
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f,
                                        string_application_header& hdr) {
  return f(meta::type_name("sa_header"), hdr.payload);
}

class string_application {
public:
  using header_type = string_application_header;

  template <class Parent>
  error init(Parent&) {
    return none;
  }

  template <class Parent>
  void handle_packet(Parent&, header_type&, span<char>) {
  }

  template <class Parent>
  void write_message(Parent& parent,
                     std::unique_ptr<net::endpoint_manager::message> msg) {
    header_type header{static_cast<uint32_t>(msg->payload.size())};
    std::vector<char> data;
    binary_serializer serializer{nullptr, data};
    serializer(header);
    serializer(msg->payload);
    parent.write_packet(data);
  }

  static expected<std::vector<char>> serialize(actor_system& sys,
                                               const type_erased_tuple& x) {
    std::vector<char> result;
    binary_serializer sink{sys, result};
    if (auto err = message::save(sink, x))
      return err;
    return result;
  }

private:
  std::vector<char> buf_;
};

class stream_string_application : public string_application {
public:
  template <class Parent>
  error init(Parent& parent) {
    parent.transport().configure_read_size(sizeof(header_type));
    return string_application::init(parent);
  }

  template <class Parent>
  void handle_data(Parent& parent, span<char> data) {
    if (await_payload_) {
      handle_packet(parent, header_, data);
    } else {
      if (header_.payload == 0)
        handle_packet(parent, header_, span<char>{});
      else
        parent.configure_read(net::receive_policy::exactly(header_.payload));
    }
  }

  template <class Manager>
  void resolve(Manager&, const std::string&, actor) {
    actor_id aid = 42;
    auto hid = "0011223344556677889900112233445566778899";
    auto nid = unbox(make_node_id(aid, hid));
    actor_config cfg;
    // TODO!
    /*auto p = make_actor<actor_proxy_impl, strong_actor_ptr>(aid, nid,
                                                            &manager.system(),
                                                            cfg, &manager);
    anon_send(listener, resolve_atom::value, std::move(path), p);*/
  }

  template <class Transport>
  void timeout(Transport&, atom_value, uint64_t) {
    // nop
  }

  void handle_error(sec) {
    // nop
  }

private:
  header_type header_;
  bool await_payload_;
};

} // namespace policy
} // namespace caf
