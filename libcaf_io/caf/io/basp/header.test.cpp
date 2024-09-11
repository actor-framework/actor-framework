// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/basp/header.hpp"

#include "caf/test/test.hpp"

#include "caf/io/basp/version.hpp"

using namespace caf::io::basp;

TEST("headers require a valid operation type") {
  header bad{static_cast<message_type>(0xFF), 0, 0, 0, 0, 0};
  check(!valid(bad));
}

TEST("server handshakes require non-zero operation data") {
  header good{message_type::server_handshake, 0, 0, version, 0, 0};
  check(valid(good));
  header bad{message_type::server_handshake, 0, 0, 0, 0, 0};
  check(!valid(bad));
}

TEST("client handshakes may not send actor IDs") {
  header good{message_type::client_handshake, 0, 0, version, 0, 0};
  check(valid(good));
  header bad1{message_type::client_handshake, 0, 0, version, 42, 42};
  check(!valid(bad1));
  header bad2{message_type::client_handshake, 0, 0, version, 42, 0};
  check(!valid(bad2));
  header bad3{message_type::client_handshake, 0, 0, version, 0, 42};
  check(!valid(bad3));
}

TEST("direct messages must have a destination and a payload") {
  header good{message_type::direct_message, 0, 256, 0, 0, 42};
  check(valid(good));
  header bad1{message_type::direct_message, 0, 0, 0, 0, 0};
  check(!valid(bad1));
  header bad2{message_type::direct_message, 0, 256, 0, 0, 0};
  check(!valid(bad2));
  header bad3{message_type::direct_message, 0, 0, 0, 0, 42};
  check(!valid(bad3));
}

TEST("routed messages must have a destination and a payload") {
  header good{message_type::routed_message, 0, 256, 0, 0, 42};
  check(valid(good));
  header bad1{message_type::routed_message, 0, 0, 0, 0, 0};
  check(!valid(bad1));
  header bad2{message_type::routed_message, 0, 256, 0, 0, 0};
  check(!valid(bad2));
  header bad3{message_type::routed_message, 0, 0, 0, 0, 42};
  check(!valid(bad3));
}

TEST("monitor messages must have a payload and may not have operation data") {
  header good{message_type::monitor_message, 0, 256, 0, 0, 0};
  check(valid(good));
  header bad1{message_type::monitor_message, 0, 0, 0, 0, 0};
  check(!valid(bad1));
  header bad2{message_type::monitor_message, 0, 256, 42, 0, 0};
  check(!valid(bad2));
}

TEST("down messages may only have a payload and a source") {
  header good{message_type::down_message, 0, 256, 0, 42, 0};
  check(valid(good));
  header bad1{message_type::down_message, 0, 256, 0, 42, 23};
  check(!valid(bad1));
  header bad2{message_type::down_message, 0, 0, 0, 42, 0};
  check(!valid(bad2));
  header bad3{message_type::down_message, 0, 256, 1, 42, 0};
  check(!valid(bad3));
}

TEST("heartbeat messages must be all-zero except for the message type") {
  header good{message_type::heartbeat, 0, 0, 0, 0, 0};
  check(valid(good));
  header bad1{message_type::heartbeat, 0, 1, 0, 0, 0};
  check(!valid(bad1));
  header bad2{message_type::heartbeat, 0, 0, 1, 0, 0};
  check(!valid(bad2));
  header bad3{message_type::heartbeat, 0, 0, 0, 1, 0};
  check(!valid(bad3));
  header bad4{message_type::heartbeat, 0, 0, 0, 0, 1};
  check(!valid(bad4));
}
