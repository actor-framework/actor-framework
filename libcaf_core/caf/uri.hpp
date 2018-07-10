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
#include <vector>

#include "caf/detail/comparable.hpp"
#include "caf/detail/unordered_flat_map.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ip_address.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

namespace caf {

/// A URI according to RFC 3986.
class uri : detail::comparable<uri>, detail::comparable<uri, string_view> {
public:
  // -- member types -----------------------------------------------------------

  /// Pointer to implementation.
  using impl_ptr = intrusive_ptr<const detail::uri_impl>;

  /// Host subcomponent of the authority component. Either an IP address or
  /// an hostname as string.
  using host_type = variant<std::string, ip_address>;

  /// Bundles the authority component of the URI, i.e., userinfo, host, and
  /// port.
  struct authority_type {
    std::string userinfo;
    host_type host;
    uint16_t port;

    inline authority_type() : port(0) {
      // nop
    }

    /// Returns whether `host` is empty, i.e., the host is not an IP address
    /// and the string is empty.
    bool empty() const noexcept {
      auto str = get_if<std::string>(&host);
      return str != nullptr && str->empty();
    }
  };

  /// Separates the query component into key-value pairs.
  using path_list = std::vector<string_view>;

  /// Separates the query component into key-value pairs.
  using query_map = detail::unordered_flat_map<std::string, std::string>;

  // -- constructors, destructors, and assignment operators --------------------

  uri();

  uri(uri&&) = default;

  uri(const uri&) = default;

  uri& operator=(uri&&) = default;

  uri& operator=(const uri&) = default;

  explicit uri(impl_ptr ptr);

  // -- properties -------------------------------------------------------------

  /// Returns whether all components of this URI are empty.
  bool empty() const noexcept;

  /// Returns the full URI as provided by the user.
  string_view str() const noexcept;

  /// Returns the scheme component.
  string_view scheme() const noexcept;

  /// Returns the authority component.
  const authority_type& authority() const noexcept;

  /// Returns the path component as provided by the user.
  string_view path() const noexcept;

  /// Returns the query component as key-value map.
  const query_map& query() const noexcept;

  /// Returns the fragment component.
  string_view fragment() const noexcept;

  // -- comparison -------------------------------------------------------------

  int compare(const uri& other) const noexcept;

  int compare(string_view x) const noexcept;

  // -- friend functions -------------------------------------------------------

  friend error inspect(caf::serializer& dst, uri& x);

  friend error inspect(caf::deserializer& src, uri& x);

private:
  impl_ptr impl_;
};

// -- related free functions ---------------------------------------------------

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, uri::authority_type& x) {
  return f(x.userinfo, x.host, x.port);
}

/// @relates uri
std::string to_string(const uri& x);

/// @relates uri
error parse(string_view str, uri& dest);

} // namespace caf
