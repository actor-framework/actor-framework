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
#include <map>
#include <set>
#include <string>
#include <vector>

#include "caf/byte.hpp"
#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/timespan.hpp"
#include "caf/timestamp.hpp"

#define CAF_MSG_TYPE_ADD_ATOM(name)                                            \
  struct name {};                                                              \
  template <class Inspector>                                                   \
  auto inspect(Inspector& f, name&) {                                          \
    return f(meta::type_name("caf::" #name));                                  \
  }                                                                            \
  constexpr name name##_v = name {                                             \
  }

namespace caf {

/// Signals the receiver to add the operands.
CAF_MSG_TYPE_ADD_ATOM(add_atom);

/// Signals the receiver to subtract the operands.
CAF_MSG_TYPE_ADD_ATOM(sub_atom);

/// Signals the receiver to multiply the operands.
CAF_MSG_TYPE_ADD_ATOM(mul_atom);

/// Signals the receiver to divide the operands.
CAF_MSG_TYPE_ADD_ATOM(div_atom);

/// Used for request operations.
CAF_MSG_TYPE_ADD_ATOM(get_atom);

/// Used for request operations.
CAF_MSG_TYPE_ADD_ATOM(put_atom);

/// Used for signalizing resolved paths.
CAF_MSG_TYPE_ADD_ATOM(resolve_atom);

/// Used for signalizing updates, e.g., in a key-value store.
CAF_MSG_TYPE_ADD_ATOM(update_atom);

/// Used for request operations.
CAF_MSG_TYPE_ADD_ATOM(delete_atom);

/// Used for response messages.
CAF_MSG_TYPE_ADD_ATOM(ok_atom);

/// Used for triggering system-level message handling.
CAF_MSG_TYPE_ADD_ATOM(sys_atom);

/// Used for signaling group subscriptions.
CAF_MSG_TYPE_ADD_ATOM(join_atom);

/// Used for signaling group unsubscriptions.
CAF_MSG_TYPE_ADD_ATOM(leave_atom);

/// Used for signaling forwarding paths.
CAF_MSG_TYPE_ADD_ATOM(forward_atom);

/// Used for buffer management.
CAF_MSG_TYPE_ADD_ATOM(flush_atom);

/// Used for I/O redirection.
CAF_MSG_TYPE_ADD_ATOM(redirect_atom);

/// Used for link requests over network.
CAF_MSG_TYPE_ADD_ATOM(link_atom);

/// Used for removing networked links.
CAF_MSG_TYPE_ADD_ATOM(unlink_atom);

/// Used for monitor requests over network.
CAF_MSG_TYPE_ADD_ATOM(monitor_atom);

/// Used for removing networked monitors.
CAF_MSG_TYPE_ADD_ATOM(demonitor_atom);

/// Used for publishing actors at a given port.
CAF_MSG_TYPE_ADD_ATOM(publish_atom);

/// Used for publishing actors at a given port.
CAF_MSG_TYPE_ADD_ATOM(publish_udp_atom);

/// Used for removing an actor/port mapping.
CAF_MSG_TYPE_ADD_ATOM(unpublish_atom);

/// Used for removing an actor/port mapping.
CAF_MSG_TYPE_ADD_ATOM(unpublish_udp_atom);

/// Used for signalizing group membership.
CAF_MSG_TYPE_ADD_ATOM(subscribe_atom);

/// Used for withdrawing group membership.
CAF_MSG_TYPE_ADD_ATOM(unsubscribe_atom);

/// Used for establishing network connections.
CAF_MSG_TYPE_ADD_ATOM(connect_atom);

/// Used for contacting a remote UDP endpoint
CAF_MSG_TYPE_ADD_ATOM(contact_atom);

/// Used for opening ports or files.
CAF_MSG_TYPE_ADD_ATOM(open_atom);

/// Used for closing ports or files.
CAF_MSG_TYPE_ADD_ATOM(close_atom);

/// Used for spawning remote actors.
CAF_MSG_TYPE_ADD_ATOM(spawn_atom);

/// Used for migrating actors to other nodes.
CAF_MSG_TYPE_ADD_ATOM(migrate_atom);

/// Used for triggering periodic operations.
CAF_MSG_TYPE_ADD_ATOM(tick_atom);

/// Used for pending out of order messages.
CAF_MSG_TYPE_ADD_ATOM(pending_atom);

/// Used as timeout type for `timeout_msg`.
CAF_MSG_TYPE_ADD_ATOM(receive_atom);

/// Used as timeout type for `timeout_msg`.
CAF_MSG_TYPE_ADD_ATOM(stream_atom);

/// Used to implement blocking_actor::wait_for.
CAF_MSG_TYPE_ADD_ATOM(wait_for_atom);

/// Can be used for testing or roundtrip time measurements in conjunction with
/// `pong_atom`.`
CAF_MSG_TYPE_ADD_ATOM(ping_atom);

/// Can be used for testing or roundtrip time measurements in conjunction with
/// `ping_atom`.`
/// Used to implement blocking_actor::wait_for.
CAF_MSG_TYPE_ADD_ATOM(pong_atom);

/// Signals that a timer elapsed.
CAF_MSG_TYPE_ADD_ATOM(timeout_atom);

/// Requests to reset a timeout or resource.
CAF_MSG_TYPE_ADD_ATOM(reset_atom);

/// Propagates an idle state.
CAF_MSG_TYPE_ADD_ATOM(idle_atom);

/// Compile-time list of all built-in types.
/// @warning Types are sorted by uniform name.
using sorted_builtin_types = detail::type_list< // uniform_type_info_map name:
  actor,                                        // @actor
  std::vector<actor>,                           // @actorvec
  actor_addr,                                   // @addr
  std::vector<actor_addr>,                      // @addrvec
  byte_buffer,                                  // @bytebuf
  std::vector<char>,                            // @charbuf
  config_value,                                 // @config_value
  down_msg,                                     // @down
  downstream_msg,                               // @downstream_msg
  error,                                        // @error
  exit_msg,                                     // @exit
  group,                                        // @group
  group_down_msg,                               // @group_down
  int16_t,                                      // @i16
  int32_t,                                      // @i32
  int64_t,                                      // @i64
  int8_t,                                       // @i8
  long double,                                  // @ldouble
  message,                                      // @message
  message_id,                                   // @message_id
  node_id,                                      // @node
  open_stream_msg,                              // @open_stream_msg
  std::string,                                  // @str
  std::map<std::string, std::string>,           // @strmap
  strong_actor_ptr,                             // @strong_actor_ptr
  std::set<std::string>,                        // @strset
  std::vector<std::string>,                     // @strvec
  timeout_msg,                                  // @timeout
  timespan,                                     // @timespan
  timestamp,                                    // @timestamp
  uint16_t,                                     // @u16
  std::u16string,                               // @u16_str
  uint32_t,                                     // @u32
  std::u32string,                               // @u32_str
  uint64_t,                                     // @u64
  uint8_t,                                      // @u8
  unit_t,                                       // @unit
  upstream_msg,                                 // @upstream_msg
  weak_actor_ptr,                               // @weak_actor_ptr
  bool,                                         // bool
  add_atom,                                     // caf::add_atom
  idle_atom,                                    // caf::idle_atom
  get_atom,                                     // caf::get_atom
  put_atom,                                     // caf::put_atom
  resolve_atom,                                 // caf::resolve_atom
  update_atom,                                  // caf::update_atom
  delete_atom,                                  // caf::delete_atom
  ok_atom,                                      // caf::ok_atom
  sys_atom,                                     // caf::sys_atom
  join_atom,                                    // caf::join_atom
  leave_atom,                                   // caf::leave_atom
  forward_atom,                                 // caf::forward_atom
  flush_atom,                                   // caf::flush_atom
  redirect_atom,                                // caf::redirect_atom
  link_atom,                                    // caf::link_atom
  unlink_atom,                                  // caf::unlink_atom
  monitor_atom,                                 // caf::monitor_atom
  mul_atom,                                     // caf::mul_atom
  demonitor_atom,                               // caf::demonitor_atom
  div_atom,                                     // caf::div_atom
  publish_atom,                                 // caf::publish_atom
  publish_udp_atom,                             // caf::publish_udp_atom
  unpublish_atom,                               // caf::unpublish_atom
  unpublish_udp_atom,                           // caf::unpublish_udp_atom
  subscribe_atom,                               // caf::subscribe_atom
  unsubscribe_atom,                             // caf::unsubscribe_atom
  connect_atom,                                 // caf::connect_atom
  contact_atom,                                 // caf::contact_atom
  close_atom,                                   // caf::close_atom
  migrate_atom,                                 // caf::migrate_atom
  open_atom,                                    // caf::open_atom
  spawn_atom,                                   // caf::spawn_atom
  sub_atom,                                     // caf::spawn_atom
  tick_atom,                                    // caf::tick_atom
  pending_atom,                                 // caf::pending_atom
  ping_atom,                                    // caf::ping_atom
  pong_atom,                                    // caf::pong_atom
  receive_atom,                                 // caf::receive_atom
  stream_atom,                                  // caf::stream_atom
  wait_for_atom,                                // caf::wait_for_atom
  double,                                       // double
  float                                         // float
  >;

/// Computes the type number for `T`.
template <class T, bool IsIntegral = std::is_integral<T>::value>
struct type_nr {
  static constexpr uint16_t value = static_cast<uint16_t>(
    detail::tl_index_of<sorted_builtin_types, T>::value + 1);
};

template <class T>
struct type_nr<T, true> {
  using type = detail::squashed_int_t<T>;
  static constexpr uint16_t value = static_cast<uint16_t>(
    detail::tl_index_of<sorted_builtin_types, type>::value + 1);
};

template <>
struct type_nr<bool, true> {
  static constexpr uint16_t value = static_cast<uint16_t>(
    detail::tl_index_of<sorted_builtin_types, bool>::value + 1);
};

/// The number of built-in types.
static constexpr size_t type_nrs
  = detail::tl_size<sorted_builtin_types>::value + 1;

/// List of all type names, indexed via `type_nr`.
CAF_CORE_EXPORT extern const char* numbered_type_names[];

template <class T>
constexpr uint16_t type_nr_v = type_nr<T>::value;

} // namespace caf
