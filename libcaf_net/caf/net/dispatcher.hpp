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

namespace caf {
namespace net {

/// Implements the interface for transport and application policies and
/// dispatches member functions either to `decorator` or `fallback`.
template <class Decorator, class Fallback>
class dispatcher {
public:
  dispatcher(Decorator& decorator, Fallback& fallback)
    : decorator_(decorator), fallback_(fallback) {
    // nop
  }

  template <class Header>
  void write_packet(const Header& header, span<const byte> payload) {
    decltype(write_packet_delegate_test<Decorator>(nullptr, header, payload)) x;
    write_packet_delegate(x, header, payload);
  }

  actor_system& system() {
    fallback_.system();
  }

  void cancel_timeout(atom_value type, uint64_t id) {
    fallback_.cancel_timeout(type, id);
  }

  void set_timeout(timestamp tout, atom_value type, uint64_t id) {
    fallback_.set_timeout(tout, type, id);
  }

  typename Fallback::transport_type& transport() {
    return fallback_.transport_;
  }

  typename Fallback::application_type& application() {
    return fallback_.application_;
  }

private:
  template <class U, class Header>
  static auto write_packet_delegate_test(U* x, const Header& hdr,
                                         span<const byte> payload)
    -> decltype(x->write_packet(hdr, payload), std::true_type());

  template <class U>
  static auto write_packet_delegate_test(...) -> std::false_type;

  template <class Header>
  void write_packet_delegate(std::true_type, const Header& header,
                             span<const byte> payload) {
    decorator_.write_packet(header, payload);
  }

  template <class Header>
  void write_packet_delegate(std::false_type, const Header& header,
                             span<const byte> payload) {
    fallback_.write_packet(header, payload);
  }

  Object& decorator_;
  Fallback& fallback_;
};

template <class Decorator, class Fallback>
dispatcher<Decorator, Fallback> make_dispatcher(Decorator& decorator,
                                                Fallback& fallback) {
  return {decorator, fallback};
}

} // namespace net
} // namespace caf
