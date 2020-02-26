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

#include <map>
#include <string>
#include <vector>

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/type_id.hpp"

// Unfortunately required, because we cannot add a forward declaration for the
// enum protol::network that we need for assigning a type ID to
// io::network::address_listing.
#include "caf/io/network/protocol.hpp"

namespace caf {

// -- templates from the parent namespace necessary for defining aliases -------

template <class> class intrusive_ptr;

namespace io {

// -- variadic templates -------------------------------------------------------

template <class... Sigs>
class typed_broker;

// -- classes ------------------------------------------------------------------

class abstract_broker;
class accept_handle;
class basp_broker;
class broker;
class connection_handle;
class datagram_servant;
class doorman;
class middleman;
class receive_policy;
class scribe;

// -- structs ------------------------------------------------------------------

struct acceptor_closed_msg;
struct acceptor_passivated_msg;
struct connection_closed_msg;
struct connection_passivated_msg;
struct data_transferred_msg;
struct datagram_sent_msg;
struct datagram_servant_closed_msg;
struct datagram_servant_passivated_msg;
struct new_connection_msg;
struct new_data_msg;
struct new_datagram_msg;

// -- aliases ------------------------------------------------------------------

using scribe_ptr = intrusive_ptr<scribe>;
using doorman_ptr = intrusive_ptr<doorman>;
using datagram_servant_ptr = intrusive_ptr<datagram_servant>;

// -- nested namespaces --------------------------------------------------------

namespace network {

class default_multiplexer;
class multiplexer;
class receive_buffer;

using address_listing = std::map<protocol::network, std::vector<std::string>>;

} // namespace network

} // namespace io
} // namespace caf

CAF_BEGIN_TYPE_ID_BLOCK(io_module, detail::io_module_begin)

  CAF_ADD_TYPE_ID(io_module, (caf::io::accept_handle));
  CAF_ADD_TYPE_ID(io_module, (caf::io::acceptor_closed_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::acceptor_passivated_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::connection_closed_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::connection_handle));
  CAF_ADD_TYPE_ID(io_module, (caf::io::connection_passivated_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::data_transferred_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::datagram_sent_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::datagram_servant_closed_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::datagram_servant_passivated_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::datagram_servant_ptr));
  CAF_ADD_TYPE_ID(io_module, (caf::io::doorman_ptr));
  CAF_ADD_TYPE_ID(io_module, (caf::io::network::address_listing));
  CAF_ADD_TYPE_ID(io_module, (caf::io::network::protocol));
  CAF_ADD_TYPE_ID(io_module, (caf::io::network::receive_buffer));
  CAF_ADD_TYPE_ID(io_module, (caf::io::new_connection_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::new_data_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::new_datagram_msg));
  CAF_ADD_TYPE_ID(io_module, (caf::io::scribe_ptr));

CAF_END_TYPE_ID_BLOCK(io_module)

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::io::doorman_ptr)
CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::io::scribe_ptr)

static_assert(caf::io_module_type_ids::end == caf::detail::io_module_end);
