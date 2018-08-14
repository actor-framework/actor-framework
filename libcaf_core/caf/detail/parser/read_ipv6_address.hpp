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

#include <algorithm>
#include <cstdint>

#include "caf/config.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/state.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/pec.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf {
namespace detail {
namespace parser {

//  IPv6address =                            6( h16 ":" ) ls32
//              /                       "::" 5( h16 ":" ) ls32
//              / [               h16 ] "::" 4( h16 ":" ) ls32
//              / [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
//              / [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
//              / [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
//              / [ *4( h16 ":" ) h16 ] "::"              ls32
//              / [ *5( h16 ":" ) h16 ] "::"              h16
//              / [ *6( h16 ":" ) h16 ] "::"
//
//  ls32        = ( h16 ":" h16 ) / IPv4address
//
//  h16         = 1*4HEXDIG

/// Reads 16 (hex) bits of an IPv6 address.
template <class Iterator, class Sentinel, class Consumer>
void read_ipv6_h16(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  uint16_t res = 0;
  size_t digits = 0;
  // Reads the a hexadecimal place.
  auto rd_hex = [&](char c) {
    ++digits;
    return add_ascii<16>(res, c);
  };
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(res);
  });
  start();
  state(init) {
    transition(read, hexadecimal_chars, rd_hex(ch), pec::integer_overflow)
  }
  term_state(read) {
    transition_if(digits < 4,
                  read, hexadecimal_chars, rd_hex(ch), pec::integer_overflow)
  }
  fin();
}

/// Reads 16 (hex) or 32 (IPv4 notation) bits of an IPv6 address.
template <class Iterator, class Sentinel, class Consumer>
void read_ipv6_h16_or_l32(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  enum mode_t { indeterminate, v6_bits, v4_octets };
  mode_t mode = indeterminate;
  uint16_t hex_res = 0;
  uint8_t dec_res = 0;
  int digits = 0;
  int octet = 0;
  // Reads a single character (dec and/or hex).
  auto rd_hex = [&](char c) {
    ++digits;
    return add_ascii<16>(hex_res, c);
  };
  auto rd_dec = [&](char c) {
    ++digits;
    return add_ascii<10>(dec_res, c);
  };
  auto rd_both = [&](char c) {
    CAF_ASSERT(mode == indeterminate);
    ++digits;
    // IPv4 octets cannot have more than 3 digits.
    if (!in_whitelist(decimal_chars, c) || !add_ascii<10>(dec_res, c))
      mode = v6_bits;
    return add_ascii<16>(hex_res, c);
  };
  auto fin_octet = [&]() {
    ++octet;
    mode = v4_octets;
    consumer.value(dec_res);
    dec_res = 0;
    digits = 0;
  };
  // Computes the result on success. Note, when reading octets, this will give
  // the consumer the final byte. Previous bytes were signaled during parsing.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      if (mode != v4_octets)
        consumer.value(hex_res);
      else
        fin_octet();
    }
  });
  start();
  state(init) {
    transition(read, hexadecimal_chars, rd_both(ch), pec::integer_overflow)
  }
  term_state(read) {
    transition_if(mode == indeterminate,
                  read, hexadecimal_chars, rd_both(ch), pec::integer_overflow)
    transition_if(mode == v6_bits,
                  read, hexadecimal_chars, rd_hex(ch), pec::integer_overflow)
    transition_if(mode != v6_bits && digits > 0, read_octet, '.', fin_octet())
  }
  state(read_octet) {
    transition(read_octet, decimal_chars, rd_dec(ch), pec::integer_overflow)
    transition_if(octet < 2 && digits > 0, read_octet, '.', fin_octet())
    transition_if(octet == 2 && digits > 0, read_last_octet, '.', fin_octet())
  }
  term_state(read_last_octet) {
    transition(read_last_octet, decimal_chars,
               rd_dec(ch), pec::integer_overflow)
  }
  fin();
}

template <class F>
struct read_ipv6_address_piece_consumer {
  F callback;

  inline void value(uint16_t x) {
    union {
      uint16_t bits;
      std::array<uint8_t, 2> bytes;
    } val;
    val.bits = to_network_order(x);
    callback(val.bytes.data(), val.bytes.size());
  }

  inline void value(uint8_t x) {
    callback(&x, 1);
  }
};

template <class F>
read_ipv6_address_piece_consumer<F> make_read_ipv6_address_piece_consumer(F f) {
  return {f};
}

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class Iterator, class Sentinel, class Consumer>
void read_ipv6_address(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  // IPv6 allows omitting blocks of zeros, splitting the string into a part
  // before the zeros (prefix) and a part after the zeros (suffix). For example,
  // ff::1 is 00FF0000000000000000000000000001
  ipv6_address::array_type prefix;
  ipv6_address::array_type suffix;
  prefix.fill(0);
  suffix.fill(0);
  // Keeps track of all bytes consumed so far, suffix and prefix combined.
  size_t filled_bytes = 0;
  auto remaining_bytes = [&] {
    return ipv6_address::num_bytes - filled_bytes;
  };
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      ipv6_address::array_type bytes;
      for (size_t i = 0; i < ipv6_address::num_bytes; ++i)
        bytes[i] = prefix[i] | suffix[i];
      ipv6_address result{bytes};
      consumer.value(result);
    }
  });
  // We need to parse 2-byte hexadecimal numbers (x) and also keep track of
  // the current writing position when reading the prefix.
  auto read_prefix = [&](uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; ++i)
      prefix[filled_bytes++] = bytes[i];
  };
  auto prefix_consumer = make_read_ipv6_address_piece_consumer(read_prefix);
  // The suffix reader rotates bytes into place, so that we can bitwise-OR
  // prefix and suffix for obtaining the full address.
  auto read_suffix = [&](uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; ++i)
      suffix[i] = bytes[i];
    std::rotate(suffix.begin(), suffix.begin() + count, suffix.end());
    filled_bytes += count;
  };
  auto suffix_consumer = make_read_ipv6_address_piece_consumer(read_suffix);
  // Utility function for promoting an IPv4 formatted input.
  auto promote_v4 = [&] {
    if (filled_bytes == 4) {
      ipv4_address::array_type bytes;
      memcpy(bytes.data(), prefix.data(), bytes.size());
      prefix = ipv6_address{ipv4_address{bytes}}.bytes();
      return true;
    }
    return false;
  };
  start();
  // Either transitions to reading leading "::" or reads the first h16. When
  // reading an l32 immediately promotes to IPv4 address and stops.
  state(init) {
    transition(rd_sep, ':')
    fsm_epsilon(read_ipv6_h16_or_l32(ps, prefix_consumer),
                maybe_has_l32, hexadecimal_chars)
  }
  // Checks whether the first call to read_ipv6_h16_or_l32 consumed
  // exactly 4 bytes. If so, we have an IPv4-formatted address.
  unstable_state(maybe_has_l32) {
    epsilon_if(promote_v4(), done)
    epsilon(rd_prefix_sep)
  }
  // Got ":" at a position where it can only be parsed as "::".
  state(rd_sep) {
    transition(has_sep, ':')
  }
  // Stops parsing after reading "::" (all-zero address) or proceeds with
  // reading the suffix.
  term_state(has_sep) {
    epsilon(rd_suffix, hexadecimal_chars)
  }
  // Read part of the prefix, i.e., everything before "::".
  state(rd_prefix) {
    fsm_epsilon_if(remaining_bytes() > 4,
                   read_ipv6_h16(ps, prefix_consumer),
                   rd_prefix_sep, hexadecimal_chars)
    fsm_epsilon_if(remaining_bytes() == 4,
                   read_ipv6_h16_or_l32(ps, prefix_consumer),
                   maybe_done, hexadecimal_chars)
    fsm_epsilon_if(remaining_bytes() == 2,
                   read_ipv6_h16(ps, prefix_consumer),
                   done, hexadecimal_chars)
  }
  // Checks whether we've read an l32 in our last call to read_ipv6_h16_or_l32,
  // in which case we're done. Otherwise continues to read the last two bytes.
  unstable_state(maybe_done) {
    epsilon_if(remaining_bytes() == 0, done);
    epsilon(rd_prefix_sep)
  }
  // Waits for ":" after reading an h16 in the prefix.
  state(rd_prefix_sep) {
    transition(rd_next_prefix, ':')
  }
  // Waits for either the second ":" or an h16/l32 after reading an ":".
  state(rd_next_prefix) {
    transition(has_sep, ':')
    epsilon(rd_prefix)
  }
  // Reads a part of the suffix.
  state(rd_suffix) {
    fsm_epsilon_if(remaining_bytes() >= 4,
                   read_ipv6_h16_or_l32(ps, suffix_consumer),
                   rd_next_suffix, hexadecimal_chars)
    fsm_epsilon_if(remaining_bytes() == 2,
                   read_ipv6_h16(ps, suffix_consumer),
                   rd_next_suffix, hexadecimal_chars)
  }
  // Reads the ":" separator between h16.
  term_state(rd_next_suffix) {
    transition(rd_suffix, ':')
  }
  // Accepts only the end-of-input, since we've read a full IP address.
  term_state(done) {
    // nop
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
