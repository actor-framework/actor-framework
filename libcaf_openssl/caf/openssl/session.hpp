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

#include <memory>

#include "caf/config.hpp"

CAF_PUSH_WARNINGS
#include <openssl/ssl.h>
CAF_POP_WARNINGS

#include "caf/actor_system.hpp"

#include "caf/io/network/native_socket.hpp"
#include "caf/io/network/default_multiplexer.hpp"

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
# define CAF_SSL_HAS_SECURITY_LEVEL
# define CAF_SSL_HAS_NON_VERSIONED_TLS_FUN
#endif

#if defined(SSL_CTX_set_ecdh_auto)
# define CAF_SSL_HAS_ECDH_AUTO
#endif

namespace caf {
namespace openssl {

using native_socket = io::network::native_socket;

using rw_state = io::network::rw_state;

class session {
public:
  session(actor_system& sys);
  ~session();

  bool init();
  rw_state read_some(size_t& result, native_socket fd, void* buf, size_t len);
  rw_state write_some(size_t& result, native_socket fd, const void* buf,
                      size_t len);
  bool try_connect(native_socket fd);
  bool try_accept(native_socket fd);

  bool must_read_more(native_socket, size_t threshold);

  const char* openssl_passphrase();

private:
  rw_state do_some(int (*f)(SSL*, void*, int), size_t& result, void* buf,
                   size_t len, const char* debug_name);
  SSL_CTX* create_ssl_context();
  std::string get_ssl_error();
  bool handle_ssl_result(int ret);

  actor_system& sys_;
  SSL_CTX* ctx_;
  SSL* ssl_;
  std::string openssl_passphrase_;
  bool connecting_;
  bool accepting_;
};

/// @relates session
using session_ptr = std::unique_ptr<session>;

/// @relates session
session_ptr make_session(actor_system& sys, native_socket fd,
                         bool from_accepted_socket);

} // namespace openssl
} // namespace caf

