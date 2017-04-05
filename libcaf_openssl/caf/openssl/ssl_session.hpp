/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_OPENSSL_SSL_SESSION_HPP
#define CAF_OPENSSL_SSL_SESSION_HPP

#include "caf/actor_system.hpp"
#include "caf/io/network/native_socket.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>

namespace caf {
namespace openssl {
using native_socket = io::network::native_socket;

class ssl_session {
public:
  ssl_session(actor_system& sys);
  ~ssl_session();

  bool read_some(size_t& result, native_socket fd, void* buf, size_t len);
  bool write_some(size_t& result, native_socket fd, const void* buf,
                  size_t len);
  bool connect(native_socket fd);
  bool try_accept(native_socket fd);
  const char* openssl_passphrase();

private:
  SSL_CTX* create_ssl_context(actor_system& sys);
  std::string get_ssl_error();
  void raise_ssl_error(std::string msg);
  bool handle_ssl_result(int ret);

  SSL_CTX* ctx;
  SSL* ssl;
  std::string openssl_passphrase_;
};

} // namespace openssl
} // namespace caf

#endif // CAF_OPENSSL_SSL_SESSION_HPP
