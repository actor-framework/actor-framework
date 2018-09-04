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

#include "caf/actor.hpp"
#include "caf/io/network/newb.hpp"
#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"

namespace caf {
namespace policy {

struct basp_header {
  uint32_t payload_len;
  actor_id from;
  actor_id to;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, basp_header& hdr) {
  return fun(meta::type_name("basp_header"), hdr.payload_len,
             hdr.from, hdr.to);
}

constexpr size_t basp_header_len = sizeof(uint32_t) + sizeof(actor_id) * 2;

struct new_basp_msg {
  basp_header header;
  char* payload;
  size_t payload_len;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun,
                                        new_basp_msg& msg) {
  return fun(meta::type_name("new_basp_message"), msg.header,
             msg.payload_len);
}

struct datagram_basp {
  static constexpr size_t header_size = basp_header_len;
  using message_type = new_basp_msg;
  using result_type = optional<message_type>;
  io::network::newb<message_type>* parent;
  message_type msg;

  datagram_basp(io::network::newb<message_type>* parent) : parent(parent) {
    // nop
  }

  error read(char* bytes, size_t count) {
    // Read header.
    if (count < basp_header_len) {
      CAF_LOG_DEBUG("not enought bytes for basp header");
      return sec::unexpected_message;
    }
    binary_deserializer bd{&parent->backend(), bytes, count};
    bd(msg.header);
    size_t payload_len = static_cast<size_t>(msg.header.payload_len);
    // Read payload.
    auto remaining = count - basp_header_len;
    // TODO: Could be `!=` ?
    if (remaining < payload_len) {
      CAF_LOG_ERROR("not enough bytes remaining to fit payload");
      return sec::unexpected_message;
    }
    msg.payload = bytes + basp_header_len;
    msg.payload_len = msg.header.payload_len;
    parent->handle(msg);
    return none;
  }

  error timeout(atom_value, uint32_t) {
    return none;
  }

  size_t write_header(io::network::byte_buffer& buf,
                      io::network::header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return header_size;
  }

  void prepare_for_sending(io::network::byte_buffer& buf,
                           size_t hstart, size_t offset, size_t plen) {
    stream_serializer<charbuf> out{&parent->backend(),
                                   buf.data() + hstart + offset,
                                   sizeof(uint32_t)};
    auto len = static_cast<uint32_t>(plen);
    out(len);
  }
};

struct stream_basp {
  static constexpr size_t header_size = basp_header_len;
  using message_type = new_basp_msg;
  using result_type = optional<message_type>;
  io::network::newb<message_type>* parent;
  message_type msg;
  bool expecting_header = true;

  stream_basp(io::network::newb<message_type>* parent) : parent(parent) {
    // TODO: this is dangerous ...
    // Maybe we need an init function that is called with `start()`?
    parent->configure_read(io::receive_policy::exactly(basp_header_len));
  }

  error read_header(char* bytes, size_t count) {
    if (count < basp_header_len)
      return sec::unexpected_message;
    binary_deserializer bd{&parent->backend(), bytes, count};
    bd(msg.header);
    size_t size = static_cast<size_t>(msg.header.payload_len);
    parent->configure_read(io::receive_policy::exactly(size));
    expecting_header = false;
    return none;
  }

  error read_payload(char* bytes, size_t count) {
    if (count < msg.header.payload_len) {
      CAF_LOG_DEBUG("buffer contains " << count << " bytes of expected "
                    << msg.header.payload_len);
      return sec::unexpected_message;
    }
    msg.payload = bytes;
    msg.payload_len = msg.header.payload_len;
    parent->handle(msg);
    expecting_header = true;
    parent->configure_read(io::receive_policy::exactly(basp_header_len));
    return none;
  }

  error read(char* bytes, size_t count) {
    if (expecting_header)
      return read_header(bytes, count);
    else
      return read_payload(bytes, count);
  }

  error timeout(atom_value, uint32_t) {
    return none;
  }

  size_t write_header(io::network::byte_buffer& buf,
                      io::network::header_writer* hw) {
    CAF_ASSERT(hw != nullptr);
    (*hw)(buf);
    return header_size;
  }

  void prepare_for_sending(io::network::byte_buffer& buf,
                           size_t hstart, size_t offset, size_t plen) {
    stream_serializer<charbuf> out{&parent->backend(),
                                   buf.data() + hstart + offset,
                                   sizeof(uint32_t)};
    auto len = static_cast<uint32_t>(plen);
    out(len);
  }
};

} // namespace policy
} // namespace caf
