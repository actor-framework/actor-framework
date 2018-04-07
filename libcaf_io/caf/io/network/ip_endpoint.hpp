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

#include <deque>
#include <vector>
#include <string>
#include <functional>

#include "caf/error.hpp"
#include "caf/meta/type_name.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/meta/load_callback.hpp"

struct sockaddr;
struct sockaddr_storage;
struct sockaddr_in;
struct sockaddr_in6;

namespace caf {
namespace io {
namespace network {

// hash for char*, see:
// - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// - http://www.isthe.com/chongo/tech/comp/fnv/index.html
// Always hash 128 bit address, for v4 we use the embedded addr.
class ep_hash {
public:
  ep_hash();
  size_t operator()(const sockaddr& sa) const noexcept;
  size_t hash(const sockaddr_in* sa) const noexcept;
  size_t hash(const sockaddr_in6* sa) const noexcept;
};

/// A hashable wrapper for a sockaddr storage.
struct ip_endpoint {
public:

  /// Default constructor for sockaddr storage which reserves memory for the
  /// internal data structure on creation.
  ip_endpoint();

  /// Move constructor.
  ip_endpoint(ip_endpoint&&) = default;

  /// Copy constructor.
  ip_endpoint(const ip_endpoint&);

  /// Destructor
  ~ip_endpoint() = default;

  /// Copy assignment operator.
  ip_endpoint& operator=(const ip_endpoint&);

  /// Move assignment operator.
  ip_endpoint& operator=(ip_endpoint&&) = default;

  /// Returns a pointer to the internal address storage.
  sockaddr* address();

  /// Returns a constant pointer to the internal address storage.
  const sockaddr* caddress() const;

  /// Returns the length of the stored address.
  size_t* length();

  /// Returns the length of the stored address.
  const size_t* clength() const;

  /// Null internal storage and length.
  void clear();

private:
  struct impl;
  struct impl_deleter { void operator()(impl*) const; };
  std::unique_ptr<impl,impl_deleter> ptr_;
};

bool operator==(const ip_endpoint& lhs, const ip_endpoint& rhs);

std::string to_string(const ip_endpoint& ep);

std::string host(const ip_endpoint& ep);

uint16_t port(const ip_endpoint& ep);

uint32_t family(const ip_endpoint& ep);

error load_endpoint(ip_endpoint& ep, uint32_t& f, std::string& h,
                    uint16_t& p, size_t& l);

error save_endpoint(ip_endpoint& ep, uint32_t& f, std::string& h,
                    uint16_t& p, size_t& l);

template <class Inspector>
typename Inspector::result_type inspect(Inspector& fun, ip_endpoint& ep) {
  uint32_t f;
  std::string h;
  uint16_t p;
  size_t l;
  if (*ep.length() > 0) {
    f = family(ep);
    h = host(ep);
    p = port(ep);
    l = *ep.length();
  }
  auto load = [&] { return load_endpoint(ep, f, h, p, l); };
  auto save = [&] { return save_endpoint(ep, f, h, p, l); };
  return fun(meta::type_name("ip_endpoint"), f, h, p, l,
             meta::load_callback(load), meta::save_callback(save));
}

} // namespace network
} // namespace io
} // namespace caf

namespace std {

template <>
struct hash<caf::io::network::ip_endpoint> {
  using argument_type = caf::io::network::ip_endpoint;
  using result_type = size_t;
  result_type operator()(const argument_type& ep) const {
    auto ptr = ep.caddress();
    return caf::io::network::ep_hash{}(*ptr);
  }
};

} // namespace std


