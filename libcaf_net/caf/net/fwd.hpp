// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/type_id.hpp"

#include <memory>
#include <vector>

namespace caf::net {

// -- templates ----------------------------------------------------------------

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
class socket_event_layer;
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

CAF_NET_EXPORT void intrusive_ptr_add_ref(socket_manager* ptr) noexcept;

CAF_NET_EXPORT void intrusive_ptr_release(socket_manager* ptr) noexcept;

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
actor_shell_ptr_t<Handle> make_actor_shell(socket_manager*);

} // namespace caf::net

namespace caf::net::octet_stream {

class lower_layer;
class policy;
class transport;
class upper_layer;

enum class errc;

} // namespace caf::net::octet_stream

namespace caf::net::lp {

class client;
class framing;
class lower_layer;
class server;
class upper_layer;

using frame = caf::chunk;

} // namespace caf::net::lp

namespace caf::net::web_socket {

class client;
class frame;
class framing;
class has_on_request;
class lower_layer;
class server;
class upper_layer;
class with_t;

enum class status : uint16_t;

} // namespace caf::net::web_socket

namespace caf::net::http {

class header;
class lower_layer;
class request;
class request_header;
class responder;
class response;
class response_header;
class route;
class router;
class server;
class upper_layer;
class with_t;

enum class method : uint8_t;
enum class status : uint16_t;

using route_ptr = intrusive_ptr<route>;

} // namespace caf::net::http

namespace caf::net::ssl {

class acceptor;
class connection;
class context;
class tcp_acceptor;
class transport;

enum class dtls;
enum class errc : uint8_t;
enum class format;
enum class tls;

} // namespace caf::net::ssl
