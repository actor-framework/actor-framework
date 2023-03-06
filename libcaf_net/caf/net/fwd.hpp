// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/type_id.hpp"

#include <memory>
#include <vector>

namespace caf::net {

// -- templates ----------------------------------------------------------------

class stream_transport;

template <class Factory>
class datagram_transport;

template <class... Sigs>
class typed_actor_shell;

template <class... Sigs>
class typed_actor_shell_ptr;

// -- classes ------------------------------------------------------------------

class actor_shell;
class actor_shell_ptr;
class middleman;
class multiplexer;
class socket_manager;
class this_host;

// -- structs ------------------------------------------------------------------

struct datagram_socket;
struct network_socket;
struct pipe_socket;
struct receive_policy;
struct socket;
struct stream_socket;
struct tcp_accept_socket;
struct tcp_stream_socket;
struct udp_datagram_socket;

// -- smart pointer aliases ----------------------------------------------------

using multiplexer_ptr = intrusive_ptr<multiplexer>;
using socket_manager_ptr = intrusive_ptr<socket_manager>;

// -- miscellaneous aliases ----------------------------------------------------

using text_buffer = std::vector<char>;

// -- factory functions --------------------------------------------------------

template <class>
struct actor_shell_ptr_oracle;

template <>
struct actor_shell_ptr_oracle<actor> {
  using type = actor_shell_ptr;
};

template <class... Sigs>
struct actor_shell_ptr_oracle<typed_actor<Sigs...>> {
  using type = typed_actor_shell_ptr<Sigs...>;
};

template <class Handle>
using actor_shell_ptr_t = typename actor_shell_ptr_oracle<Handle>::type;

template <class Handle = caf::actor>
actor_shell_ptr_t<Handle>
make_actor_shell(actor_system&, async::execution_context_ptr);

} // namespace caf::net
