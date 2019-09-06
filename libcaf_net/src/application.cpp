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

#include "caf/net/basp/application.hpp"

#include <vector>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/byte.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/net/basp/constants.hpp"
#include "caf/net/basp/ec.hpp"
#include "caf/none.hpp"
#include "caf/serializer_impl.hpp"
#include "caf/type_erased_tuple.hpp"

namespace caf {
namespace net {
namespace basp {

expected<std::vector<byte>> application::serialize(actor_system& sys,
                                                   const type_erased_tuple& x) {
  std::vector<byte> result;
  serializer_impl<std::vector<byte>> sink{sys, result};
  if (auto err = message::save(sink, x))
    return err;
  return result;
}

error application::handle(span<const byte> bytes) {
  switch (state_) {
    case connection_state::await_handshake_header: {
      if (bytes.size() != header_size)
        return ec::unexpected_number_of_bytes;
      hdr_ = header::from_bytes(bytes);
      if (hdr_.type != message_type::handshake)
        return ec::missing_handshake;
      if (hdr_.operation_data != version)
        return ec::version_mismatch;
      if (hdr_.payload_len == 0)
        return ec::missing_payload;
      state_ = connection_state::await_handshake_payload;
      return none;
    }
    case connection_state::await_handshake_payload: {
      node_id nid;
      std::vector<std::string> app_ids;
      binary_deserializer source{system(), bytes};
      if (auto err = source(nid, app_ids))
        return err;
      if (!nid || app_ids.empty())
        return ec::invalid_handshake;
      auto ids = get_or(system().config(), "middleman.app-identifiers",
                        defaults::middleman::app_identifiers);
      auto predicate = [=](const std::string& x) {
        return std::find(ids.begin(), ids.end(), x) != ids.end();
      };
      if (std::none_of(app_ids.begin(), app_ids.end(), predicate))
        return ec::app_identifiers_mismatch;
      state_ = connection_state::await_header;
      return none;
    }
    case connection_state::await_header: {
      if (bytes.size() != header_size)
        return ec::unexpected_number_of_bytes;
      hdr_ = header::from_bytes(bytes);
      if (hdr_.payload_len == 0)
        return handle(hdr_, span<const byte>{});
      else
        state_ = connection_state::await_payload;
      return none;
    }
    case connection_state::await_payload: {
      if (bytes.size() != hdr_.payload_len)
        return ec::unexpected_number_of_bytes;
      return handle(hdr_, bytes);
    }
    default:
      return ec::illegal_state;
  }
}

error application::handle(header hdr, span<const byte>) {
  switch (hdr.type) {
    case message_type::heartbeat:
      return none;
    default:
      return ec::unimplemented;
  }
}

error application::handle_handshake(header hdr, span<const byte> payload) {
  if (hdr.type != message_type::handshake)
    return ec::missing_handshake;
  if (hdr.operation_data != version)
    return ec::version_mismatch;
  binary_deserializer source{nullptr, payload};
  node_id peer_id;
  std::vector<std::string> app_ids;
  if (auto err = source(peer_id, app_ids))
    return err;
  // TODO: verify peer_id and app_ids
  peer_id_ = std::move(peer_id);
  state_ = connection_state::await_header;
  return none;
}

error application::generate_handshake() {
  serializer_impl<buffer_type> sink{system(), buf_};
  return sink(system().node(),
              get_or(system().config(), "middleman.app-identifiers",
                     defaults::middleman::app_identifiers));
}

} // namespace basp
} // namespace net
} // namespace caf
