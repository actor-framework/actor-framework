/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_IO_BASP_HPP
#define CAF_IO_BASP_HPP

#include <cstdint>

#include "caf/node_id.hpp"
#include "caf/abstract_actor.hpp"
#include "caf/binary_serializer.hpp"
#include "caf/binary_deserializer.hpp"

namespace caf {
namespace io {
namespace basp {

/**
 * @brief The header of a Binary Actor System Protocol (BASP) message.
 *
 * A BASP header consists of a routing part, i.e., source and
 * destination, as well as an operation and operation data. Several
 * message types consist of only a header.
 */
struct header {
    node_id source_node;
    node_id dest_node;
    actor_id source_actor;
    actor_id dest_actor;
    uint32_t payload_len;
    uint32_t operation;
    uint64_t operation_data;
};

/**
 * @brief The current BASP version. Different BASP versions will not
 *        be able to exchange messages.
 */
constexpr uint64_t version = 1;

/**
 * @brief Size of a BASP header in serialized form
 */
constexpr size_t header_size =
    node_id::host_id_size * 2 + sizeof(uint32_t) * 2 +
    sizeof(actor_id) * 2 + sizeof(uint32_t) * 2 + sizeof(uint64_t);

inline bool valid(const node_id& val) {
    return val != invalid_node_id;
}

inline bool invalid(const node_id& val) {
    return !valid(val);
}

template<typename T>
inline bool zero(T val) {
    return val == 0;
}

template<typename T>
inline bool nonzero(T aid) {
    return !zero(aid);
}

/**
 * @brief Send from server, i.e., the node with a published actor, to client,
 *        i.e., node that initiates a new connection using remote_actor().
 *
 * @param source_node     ID of server
 * @param dest_node       invalid
 * @param source_actor    Optional: ID of published actor
 * @param dest_actor      0
 * @param payload_len     Optional: size of actor id + interface definition
 * @param operation_data  BASP version of the server
 */
constexpr uint32_t server_handshake = 0x00;

inline bool server_handshake_valid(const header& hdr) {
    return    valid(hdr.source_node)
           && invalid(hdr.dest_node)
           && zero(hdr.dest_actor)
           && nonzero(hdr.operation_data)
           && (   (nonzero(hdr.source_actor) && nonzero(hdr.payload_len))
               || (zero(hdr.source_actor) && zero(hdr.payload_len)));
}

/**
 * @brief Send from client to server after it has successfully received the
 *        server_handshake to establish the connection.
 *
 * @param source_node     ID of client
 * @param dest_node       ID of server
 * @param source_actor    0
 * @param dest_actor      0
 * @param payload_len     0
 * @param operation_data  0
 */
constexpr uint32_t client_handshake = 0x01;

inline bool client_handshake_valid(const header& hdr) {
    return    valid(hdr.source_node)
           && valid(hdr.dest_node)
           && hdr.source_node != hdr.dest_node
           && zero(hdr.source_actor)
           && zero(hdr.dest_actor)
           && zero(hdr.payload_len)
           && zero(hdr.operation_data);
}

/**
 * @brief Transmits a message from source_node:source_actor to
 *        dest_node:dest_actor.
 *
 * @param source_node     ID of sending node (invalid in case of anon_send)
 * @param dest_node       ID of receiving node
 * @param source_actor    ID of sending actor (invalid in case of anon_send)
 * @param dest_actor      ID of receiving actor, must not be invalid
 * @param payload_len     size of serialized message object, must not be 0
 * @param operation_data  message ID (0 for asynchronous messages)
 */
constexpr uint32_t dispatch_message = 0x02;

inline bool dispatch_message_valid(const header& hdr) {
    return    valid(hdr.dest_node)
           && nonzero(hdr.dest_actor)
           && nonzero(hdr.payload_len);
}

/**
 * @brief Informs the receiving node that the sending node has created a proxy
 *        instance for one of its actors. Causes the receiving node to attach
 *        a functor to the actor that triggers a kill_proxy_instance
 *        message on termination.
 *
 * @param source_node     ID of sending node
 * @param dest_node       ID of receiving node
 * @param source_actor    0
 * @param dest_actor      ID of monitored actor
 * @param payload_len     0
 * @param operation_data  0
 */
constexpr uint32_t announce_proxy_instance = 0x03;

inline bool announce_proxy_instance_valid(const header& hdr) {
    return    valid(hdr.source_node)
           && valid(hdr.dest_node)
           && hdr.source_node != hdr.dest_node
           && zero(hdr.source_actor)
           && nonzero(hdr.dest_actor)
           && zero(hdr.payload_len)
           && zero(hdr.operation_data);
}

/**
 * @brief Informs the receiving node that it has a proxy for an actor
 *        that has been terminated.
 *
 * @param source_node     ID of sending node
 * @param dest_node       ID of receiving node
 * @param source_actor    ID of monitored actor
 * @param dest_actor      0
 * @param payload_len     0
 * @param operation_data  exit reason (uint32)
 */
constexpr uint32_t kill_proxy_instance = 0x04;

inline bool kill_proxy_instance_valid(const header& hdr) {
    return    valid(hdr.source_node)
           && valid(hdr.dest_node)
           && hdr.source_node != hdr.dest_node
           && nonzero(hdr.source_actor)
           && zero(hdr.dest_actor)
           && zero(hdr.payload_len)
           && nonzero(hdr.operation_data);
}

/**
 * @brief Checks whether given header is valid.
 */
inline bool valid(header& hdr) {
    switch (hdr.operation) {
        default:
            return false; // invalid operation field
        case server_handshake:
            return server_handshake_valid(hdr);
        case client_handshake:
            return client_handshake_valid(hdr);
        case dispatch_message:
            return dispatch_message_valid(hdr);
        case announce_proxy_instance:
            return announce_proxy_instance_valid(hdr);
        case kill_proxy_instance:
            return kill_proxy_instance_valid(hdr);
    }
}

} // namespace basp
} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_HPP
