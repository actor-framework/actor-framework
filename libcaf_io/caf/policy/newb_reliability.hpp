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

#include <cstdint>
#include <unordered_map>
#include <chrono>

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/error.hpp"
#include "caf/io/network/newb.hpp"

namespace caf {
namespace policy {

using id_type = uint16_t;
using reliability_atom = atom_constant<atom("ordering")>;

struct reliability_header {
  id_type id;
  // TODO: or build an enum for this (ack, normal, retransmit)?
  bool is_ack;
};

constexpr size_t reliability_header_len = sizeof(id_type) +
                                          sizeof(bool);

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, reliability_header& hdr) {
  return fun(meta::type_name("reliability_header"), hdr.id, hdr.is_ack);
}

// TODO: Currently must be first layer. This is simpler for retransmitting and
//  saving the sent data.
template <class Next>
struct reliability {
  static constexpr size_t header_size = reliability_header_len;
  using message_type = typename Next::message_type;
  using result_type = typename Next::result_type;
  id_type id_write = 0;
  // TODO: Make this configurable.
  std::chrono::milliseconds retransmit_to = std::chrono::milliseconds(100);
  io::network::newb<message_type>* parent;
  Next next;
  std::unordered_map<id_type, io::network::byte_buffer> unacked;

  reliability(io::network::newb<message_type>* parent)
      : parent(parent),
        next(parent) {
    // nop
  }

  error read(char* bytes, size_t count) {
    if (count < header_size)
      return sec::unexpected_message;
    reliability_header hdr;
    binary_deserializer bd(&parent->backend(), bytes, count);
    bd(hdr);
    if (hdr.is_ack) {
      // TODO: Cancel timeout.
      unacked.erase(hdr.id);
    } else {
      // Send ack.
      auto& buf = parent->wr_buf();
      binary_serializer bs(&parent->backend(), buf);
      bs(reliability_header{hdr.id, true});
      parent->flush();
      // Handle packet.
      auto res = next.read(bytes + header_size, count - header_size);
      if (res)
        return res;
    }
    return none;
  }

  error timeout(atom_value atm, uint32_t id) {
    if (atm == reliability_atom::value) {
      id_type retransmit_id = static_cast<id_type>(id);
      if (unacked.count(retransmit_id) > 0) {
        // Retransmit the packet.
        auto& packet = unacked[retransmit_id];
        auto& buf = parent->wr_buf();
        buf.insert(buf.begin(), packet.begin(), packet.end());
        parent->flush();
        parent->set_timeout(retransmit_to, reliability_atom::value, retransmit_id);
      }
      return none;
    }
    return next.timeout(atm, id);
  }

  void write_header(io::network::byte_buffer& buf,
                    io::network::header_writer* hw) {
    binary_serializer bs(&parent->backend(), buf);
    bs(reliability_header{id_write, false});
    next.write_header(buf, hw);
    return;
  }

  void prepare_for_sending(io::network::byte_buffer& buf,
                           size_t hstart, size_t offset, size_t plen) {
    next.prepare_for_sending(buf, hstart, offset + header_size, plen);
    // Set timeout for retransmission.
    parent->set_timeout(retransmit_to, reliability_atom::value, id_write);
    // Add to unacked.
    unacked.emplace(id_write,
                    io::network::byte_buffer(buf.begin() + hstart, buf.end()));
    id_write += 1;
  }
};

} // namespace policy
} // namespace caf
