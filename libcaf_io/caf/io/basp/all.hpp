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

#include "caf/io/basp/buffer_type.hpp"
#include "caf/io/basp/connection_state.hpp"
#include "caf/io/basp/endpoint_context.hpp"
#include "caf/io/basp/header.hpp"
#include "caf/io/basp/instance.hpp"
#include "caf/io/basp/message_type.hpp"
#include "caf/io/basp/routing_table.hpp"
#include "caf/io/basp/version.hpp"

/// @defgroup BASP Binary Actor Sytem Protocol
///
/// # Protocol Overview
///
/// The "Binary Actor Sytem Protocol" (BASP) is **not** a network protocol.
/// It is a specification for the "Remote Method Invocation" (RMI) interface
/// used by distributed instances of CAF. The purpose of BASP is unify the
/// structure of RMI calls in order to simplify processing and implementation.
/// Hence, BASP is independent of any underlying network technology,
/// and assumes a reliable communication channel.
///
///
/// The RMI interface of CAF enables network-transparent monitoring and linking
/// as well as global message dispatching to actors running on different nodes.
///
/// ![](basp_overview.png)
///
/// The figure above illustrates the phyiscal as well as the logical view
/// of a distributed CAF application. Note that the actors used for the
/// BASP communication ("BASP Brokers") are not part of the logical system
/// view and are in fact not visible to other actors. A BASP Broker creates
/// proxy actors that represent actors running on different nodes. It is worth
/// mentioning that two instances of CAF running on the same physical machine
/// are considered two different nodes in BASP.
///
/// BASP has two objectives:
///
/// - **Forward messages sent to a proxy to the actor it represents**
///
///   Whenever a proxy instance receives a message, it forwards this message to
///   its parent (a BASP Broker). This message is then serialized and forwarded
///   over the network. If no direct connection between the node sending the
///   message and the node receiving it exists, intermediate BASP Brokers will
///   forward it until it the message reaches its destination.
///
/// - **Synchronize the state of an actor with all of its proxies**.
///
///   Whenever a node learns the address of a remotely running actor, it
///   creates  Ma local proxy instance representing this actor and sends an
///   `announce_proxy_instance` to the node hosting the actor. Whenever an actor
///   terminates, the hosting node sends `down_message` messages to all
///   nodes that have a proxy for this actor. This enables network-transparent
///   actor monitoring. There are two possible ways addresses can be learned:
///
///   + A client connects to a remotely running (published) actor via
///     `remote_actor`. In this case, the `server_handshake` will contain the
///     address of the published actor.
///
///   + Receiving `dispatch_message`. Whenever an actor message arrives,
///     it usually contains the address of the sender. Further, the message
///     itself can contain addresses to other actors that the BASP Broker
///     will get aware of while deserializing the message object
///     from the payload.
///
/// # Node IDs
///
/// The ID of a node consists of a 120 bit hash and the process ID. Note that
/// we use "node" as a synonym for "CAF instance". The hash is generated from
/// "low-level" characteristics of a machine such as the UUID of the root
/// file system and available MAC addresses. The only purpose of the node ID
/// is to generate a network-wide unique identifier. By adding the process ID,
/// CAF disambiguates multiple instances running on the same phyisical machine.
///
/// # Header Format
///
/// ![](basp_header.png)
///
/// - **Operation ID**: 4 bytes.
///
///   This field indicates what BASP function this datagram represents.
///   The value is an `uint32_t` representation of `message_type`.
///
/// - **Payload Length**: 4 bytes.
///
///   The length of the data following this header as `uint32_t`,
///   measured in bytes.
///
/// - **Operation Data**: 8 bytes.
///
///   This field contains.
///
/// - **Source Node ID**: 18 bytes.
///
///   The address of the source node. See [Node IDs](# Node IDs).
///
/// - **Destination Node ID**: 18 bytes.
///
///   The address of the destination node. See [Node IDs](# Node IDs).
///   Upon receiving this datagram, a BASP Broker compares this node ID
///   to its own ID. On a mismatch, it selects the next hop and forwards
///   this datagram unchanged.
///
/// - **Source Actor ID**: 4 bytes.
///
///   This field contains the ID of the sending actor or 0 for anonymously
///   sent messages. The *full address* of an actor is the combination of
///   the node ID and the actor ID. Thus, every actor can be unambigiously
///   identified by combining these two IDs.
///
/// - **Destination Actor ID**: 4 bytes.
///
///   This field contains the ID of the receiving actor or 0 for BASP
///   functions that do not require
///
/// # Example
///
/// The following diagram models a distributed application
/// with three nodes. The pseudo code for the application can be found
/// in the three grey boxes, while the resulting BASP messaging
/// is shown in UML sequence diagram notation. More details about
/// individual BASP message types can be found in the documentation
/// of {@link message_type} below.
///
/// ![](basp_sequence.png)

