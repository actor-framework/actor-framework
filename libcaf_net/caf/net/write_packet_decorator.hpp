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
/// dispatches member functions either to `decorator` or `parent`.
template <class Object, class Parent>
class write_packet_decorator {
public:
  dispatcher(Object& object, Parent& parent)
    : object_(object), parent_(parent) {
    // nop
  }

  template <class Header>
  void write_packet(const Header& header, span<const byte> payload) {
    object_.write_packet(parent_, header, payload);
  }

  actor_system& system() {
    parent_.system();
  }

  void cancel_timeout(atom_value type, uint64_t id) {
    parent_.cancel_timeout(type, id);
  }

  void set_timeout(timestamp tout, atom_value type, uint64_t id) {
    parent_.set_timeout(tout, type, id);
  }

  typename parent::transport_type& transport() {
    return parent_.transport_;
  }

  typename parent::application_type& application() {
    return parent_.application_;
  }

private:
  Object& object_;
  Parent& parent_;
};

template <class Object, class Parent>
write_packet_decorator<Object, Parent>
make_write_packet_decorator(Object& object, Parent& parent) {
  return {object, parent};
}

} // namespace net
} // namespace caf
