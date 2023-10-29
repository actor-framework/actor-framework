// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/io_export.hpp"
#include "caf/error.hpp"
#include "caf/sec.hpp"

#include <deque>
#include <functional>
#include <string>
#include <vector>

struct sockaddr;
struct sockaddr_storage;
struct sockaddr_in;
struct sockaddr_in6;

namespace caf::io::network {

// hash for char*, see:
// - https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// - http://www.isthe.com/chongo/tech/comp/fnv/index.html
// Always hash 128 bit address, for v4 we use the embedded addr.
class CAF_IO_EXPORT ep_hash {
public:
  ep_hash();
  size_t operator()(const sockaddr& sa) const noexcept;
  size_t hash(const sockaddr_in* sa) const noexcept;
  size_t hash(const sockaddr_in6* sa) const noexcept;
};

/// A hashable wrapper for a sockaddr storage.
struct CAF_IO_EXPORT ip_endpoint {
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
  struct impl_deleter {
    void operator()(impl*) const;
  };
  std::unique_ptr<impl, impl_deleter> ptr_;
};

CAF_IO_EXPORT bool operator==(const ip_endpoint& lhs, const ip_endpoint& rhs);

inline bool operator!=(const ip_endpoint& lhs, const ip_endpoint& rhs) {
  return !(lhs == rhs);
}

CAF_IO_EXPORT std::string to_string(const ip_endpoint& ep);

CAF_IO_EXPORT std::string host(const ip_endpoint& ep);

CAF_IO_EXPORT uint16_t port(const ip_endpoint& ep);

CAF_IO_EXPORT uint32_t family(const ip_endpoint& ep);

CAF_IO_EXPORT bool is_ipv4(const ip_endpoint& ep);

CAF_IO_EXPORT bool is_ipv6(const ip_endpoint& ep);

CAF_IO_EXPORT error_code<sec> load_endpoint(ip_endpoint& ep, uint32_t& f,
                                            std::string& h, uint16_t& p,
                                            size_t& l);

CAF_IO_EXPORT error_code<sec> save_endpoint(ip_endpoint& ep, uint32_t& f,
                                            std::string& h, uint16_t& p,
                                            size_t& l);

template <class Inspector>
bool inspect(Inspector& f, ip_endpoint& x) {
  uint32_t fam;
  std::string h;
  uint16_t p;
  size_t l;
  if constexpr (!Inspector::is_loading) {
    if (*x.length() > 0) {
      fam = family(x);
      h = host(x);
      p = port(x);
      l = *x.length();
    }
  }
  auto load = [&] {
    if (auto err = load_endpoint(x, fam, h, p, l)) {
      f.set_error(err);
      return false;
    }
    return true;
  };
  auto save = [&] {
    if (auto err = save_endpoint(x, fam, h, p, l)) {
      f.set_error(err);
      return false;
    }
    return true;
  };
  return f.object(x).on_load(load).on_save(save).fields(f.field("family", fam),
                                                        f.field("host", h),
                                                        f.field("port", p),
                                                        f.field("length", l));
}

} // namespace caf::io::network

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
