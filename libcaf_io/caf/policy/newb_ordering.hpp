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

#include <chrono>
#include <cstdint>
#include <map>

#include "caf/binary_deserializer.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/defaults.hpp"
#include "caf/error.hpp"
#include "caf/io/network/newb.hpp"

namespace caf {
namespace policy {

using sequence_type = uint16_t;
using ordering_atom = atom_constant<atom("ordering")>;

} // namespace policy
} // namespace caf

namespace  {

bool is_greater(caf::policy::sequence_type lhs, caf::policy::sequence_type rhs,
                caf::policy::sequence_type max_distance
                  = std::numeric_limits<caf::policy::sequence_type>::max() / 2) {
  // distance between lhs and rhs is smaller than max_distance.
  return ((lhs > rhs) && (lhs - rhs <= max_distance)) ||
         ((lhs < rhs) && (rhs - lhs > max_distance));
}

struct sequence_comperator {
    bool operator()(const caf::policy::sequence_type& lhs,
                    const caf::policy::sequence_type& rhs) const {
        return is_greater(rhs, lhs);
    }
};

} // namespace anonymous

namespace caf {
namespace policy {

struct ordering_header {
  sequence_type seq;
};

constexpr size_t ordering_header_len = sizeof(sequence_type);

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, ordering_header& hdr) {
  return fun(meta::type_name("ordering_header"), hdr.seq);
}

template <class Next>
struct ordering {
  static constexpr size_t header_size = ordering_header_len;
  using message_type = typename Next::message_type;
  using result_type = typename Next::result_type;
  sequence_type seq_read = 0;
  sequence_type seq_write = 0;
  size_t max_pending_messages;
  bool use_timeouts;
  std::chrono::milliseconds pending_to = std::chrono::milliseconds(100);
  io::network::newb<message_type>* parent;
  Next next;
  std::map<sequence_type, std::vector<char>, sequence_comperator> pending;

  ordering(io::network::newb<message_type>* parent, bool use_timeouts = true)
      : max_pending_messages(get_or(parent->config(),
                                    "middleman.max-pending-messages",
                                    caf::defaults::middleman::max_pending_messages)),
        use_timeouts(use_timeouts),
        parent(parent),
        next(parent) {
    // nop
  }

  error deliver_pending() {
    if (pending.empty())
      return none;
    while (pending.count(seq_read) > 0) {
      auto& buf = pending[seq_read];
      auto res = next.read(buf.data(), buf.size());
      pending.erase(seq_read);
      seq_read += 1;
      // TODO: Cancel timeout.
      if (res)
        return res;
    }
    return none;
  }

  error add_pending(char* bytes, size_t count, sequence_type seq) {
    pending[seq] = std::vector<char>(bytes + header_size, bytes + count);
    if (use_timeouts)
      parent->set_timeout(pending_to, ordering_atom::value, seq);
    if (pending.size() > max_pending_messages) {
      seq_read = pending.begin()->first;
      return deliver_pending();
    }
    return none;
  }

  error read(char* bytes, size_t count) {
    if (count < header_size)
      return sec::unexpected_message;
    ordering_header hdr;
    binary_deserializer bd(&parent->backend(), bytes, count);
    bd(hdr);
    if (hdr.seq == seq_read) {
      seq_read += 1;
      auto res = next.read(bytes + header_size, count - header_size);
      if (res)
        return res;
      return deliver_pending();
    } else if (is_greater(hdr.seq, seq_read)) {
      add_pending(bytes, count, hdr.seq);
      return none;
    }
    return none;
  }

  error timeout(atom_value atm, uint32_t id) {
    if (atm == ordering_atom::value) {
      error err = none;
      sequence_type seq = static_cast<sequence_type>(id);
      if (pending.count(seq) > 0) {
        seq_read = static_cast<sequence_type>(seq);
        err = deliver_pending();
      }
      return err;
    }
    return next.timeout(atm, id);
  }

  void write_header(io::network::byte_buffer& buf,
                    io::network::header_writer* hw) {
    binary_serializer bs(&parent->backend(), buf);
    bs(ordering_header{seq_write});
    seq_write += 1;
    next.write_header(buf, hw);
    return;
  }

  void prepare_for_sending(io::network::byte_buffer& buf,
                           size_t hstart, size_t offset, size_t plen) {
    next.prepare_for_sending(buf, hstart, offset + header_size, plen);
  }
};

} // namespace policy
} // namespace caf
