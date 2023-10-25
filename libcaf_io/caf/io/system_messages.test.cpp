// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/system_messages.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"

using namespace caf;
using namespace caf::io;

namespace {

WITH_FIXTURE(test::fixture::deterministic) {

TEST("new_connection_msg is serializable") {
  auto dummy_connection = connection_handle::from_int(42);
  auto dummy_acceptor = accept_handle::from_int(43);
  auto msg1 = new_connection_msg{dummy_acceptor, dummy_connection};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value())) {
    check_eq(msg1.handle, msg2->handle);
    check_eq(msg1.source, msg2->source);
  }
}

TEST("new_data_msg is serializable") {
  SECTION("empty buffer") {
    auto dummy_connection = connection_handle::from_int(42);
    auto msg1 = new_data_msg{dummy_connection, byte_buffer{}};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.handle, msg2->handle);
      check_eq(msg1.buf, msg2->buf);
    }
  }
  SECTION("non-empty buffer") {
    auto dummy_connection = connection_handle::from_int(42);
    auto msg1 = new_data_msg{
      dummy_connection, byte_buffer{std::byte{1}, std::byte{2}, std::byte{3}}};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.handle, msg2->handle);
      check_eq(msg1.buf, msg2->buf);
    }
  }
}

TEST("data_transferred_msg is serializable") {
  auto dummy_connection = connection_handle::from_int(42);
  auto msg1 = data_transferred_msg{dummy_connection, 1, 2};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value())) {
    check_eq(msg1.handle, msg2->handle);
    check_eq(msg1.written, msg2->written);
    check_eq(msg1.remaining, msg2->remaining);
  }
}

TEST("connection_closed_msg is serializable") {
  auto dummy_connection = connection_handle::from_int(42);
  auto msg1 = connection_closed_msg{dummy_connection};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value()))
    check_eq(msg1.handle, msg2->handle);
}

TEST("acceptor_closed_msg is serializable") {
  auto dummy_acceptor = accept_handle::from_int(42);
  auto msg1 = acceptor_closed_msg{dummy_acceptor};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value()))
    check_eq(msg1.handle, msg2->handle);
}

TEST("connection_passivated_msg is serializable") {
  auto dummy_connection = connection_handle::from_int(42);
  auto msg1 = connection_passivated_msg{dummy_connection};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value()))
    check_eq(msg1.handle, msg2->handle);
}

TEST("acceptor_passivated_msg is serializable") {
  auto dummy_acceptor = accept_handle::from_int(42);
  auto msg1 = acceptor_passivated_msg{dummy_acceptor};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value()))
    check_eq(msg1.handle, msg2->handle);
}

TEST("new_datagram_msg is serializable") {
  SECTION("empty buffer") {
    auto dummy_datagram = datagram_handle::from_int(42);
    auto msg1 = new_datagram_msg{dummy_datagram, network::receive_buffer{}};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.handle, msg2->handle);
      check_eq(msg1.buf.empty(), msg2->buf.empty());
    }
  }
  SECTION("non-empty buffer") {
    auto dummy_datagram = datagram_handle::from_int(42);
    auto dummy_buffer = network::receive_buffer{3};
    dummy_buffer.insert(dummy_buffer.begin(), '0');
    dummy_buffer.insert(dummy_buffer.begin(), '1');
    dummy_buffer.insert(dummy_buffer.begin(), '2');
    auto msg1 = new_datagram_msg{dummy_datagram, dummy_buffer};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.handle, msg2->handle);
      if (check_eq(msg1.buf.size(), msg2->buf.size())) {
        for (size_t i = 0; i < msg1.buf.size(); ++i)
          check_eq(*(msg1.buf.begin() + i), *(msg2->buf.begin() + i));
      }
    }
  }
}

TEST("datagram_sent_msg is serializable") {
  SECTION("empty buffer") {
    auto dummy_datagram = datagram_handle::from_int(42);
    auto msg1 = datagram_sent_msg{dummy_datagram, 1, byte_buffer{}};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.handle, msg2->handle);
      check_eq(msg1.written, msg2->written);
      check_eq(msg1.buf, msg2->buf);
    }
  }
  SECTION("non-empty buffer") {
    auto dummy_datagram = datagram_handle::from_int(42);
    auto msg1 = datagram_sent_msg{
      dummy_datagram, 1, byte_buffer{std::byte{1}, std::byte{2}, std::byte{3}}};
    auto msg2 = serialization_roundtrip(msg1);
    if (check(msg2.has_value())) {
      check_eq(msg1.handle, msg2->handle);
      check_eq(msg1.written, msg2->written);
      check_eq(msg1.buf, msg2->buf);
    }
  }
}

TEST("datagram_servant_passivated_msg is serializable") {
  auto dummy_datagram = datagram_handle::from_int(42);
  auto msg1 = datagram_servant_passivated_msg{dummy_datagram};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value()))
    check_eq(msg1.handle, msg2->handle);
}

TEST("dataram_servant_closed_msg is serializable") {
  auto dummy_datagram = datagram_handle::from_int(42);
  auto msg1 = datagram_servant_closed_msg{std::vector{dummy_datagram}};
  auto msg2 = serialization_roundtrip(msg1);
  if (check(msg2.has_value())) {
    if (check_eq(msg1.handles.size(), msg2->handles.size())) {
      for (size_t i = 0; i < msg1.handles.size(); ++i)
        check_eq(msg1.handles[i], msg2->handles[i]);
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
