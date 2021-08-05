// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <vector>

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/unordered_flat_map.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/inspector_access.hpp"
#include "caf/intrusive_cow_ptr.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ip_address.hpp"
#include "caf/make_counted.hpp"
#include "caf/string_view.hpp"
#include "caf/variant.hpp"

namespace caf {

/// A URI according to RFC 3986.
class CAF_CORE_EXPORT uri : detail::comparable<uri>,
                            detail::comparable<uri, string_view> {
public:
  // -- friends -

  template <class>
  friend struct inspector_access;

  // -- member types -----------------------------------------------------------

  /// Host subcomponent of the authority component. Either an IP address or
  /// an hostname as string.
  using host_type = variant<std::string, ip_address>;

  /// Bundles the authority component of the URI, i.e., userinfo, host, and
  /// port.
  struct authority_type {
    std::string userinfo;
    host_type host;
    uint16_t port;

    authority_type() : port(0) {
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

  class CAF_CORE_EXPORT impl_type {
  public:
    // -- constructors, destructors, and assignment operators ------------------

    impl_type();

    impl_type(const impl_type&) = delete;

    impl_type& operator=(const impl_type&) = delete;

    // -- member variables -----------------------------------------------------

    /// Null-terminated buffer for holding the string-representation of the URI.
    std::string str;

    /// Scheme component.
    std::string scheme;

    /// Assembled authority component.
    uri::authority_type authority;

    /// Path component.
    std::string path;

    /// Query component as key-value pairs.
    uri::query_map query;

    /// The fragment component.
    std::string fragment;

    // -- properties -----------------------------------------------------------

    bool valid() const noexcept {
      return !scheme.empty() && (!authority.empty() || !path.empty());
    }

    bool unique() const noexcept {
      return rc_.load() == 1;
    }

    // -- modifiers ------------------------------------------------------------

    /// Assembles the human-readable string representation for this URI.
    void assemble_str();

    // -- friend functions -----------------------------------------------------

    friend void intrusive_ptr_add_ref(const impl_type* p) {
      p->rc_.fetch_add(1, std::memory_order_relaxed);
    }

    friend void intrusive_ptr_release(const impl_type* p) {
      if (p->rc_ == 1 || p->rc_.fetch_sub(1, std::memory_order_acq_rel) == 1)
        delete p;
    }

  private:
    // -- member variables -----------------------------------------------------

    mutable std::atomic<size_t> rc_;
  };

  /// Pointer to implementation.
  using impl_ptr = intrusive_ptr<impl_type>;

  // -- constructors, destructors, and assignment operators --------------------

  uri();

  uri(uri&&) = default;

  uri(const uri&) = default;

  uri& operator=(uri&&) = default;

  uri& operator=(const uri&) = default;

  explicit uri(impl_ptr ptr);

  // -- properties -------------------------------------------------------------

  /// Returns whether all components of this URI are empty.
  bool empty() const noexcept {
    return str().empty();
  }

  /// Returns whether the URI contains valid content.
  bool valid() const noexcept {
    return !empty();
  }

  /// Returns the full URI as provided by the user.
  string_view str() const noexcept {
    return impl_->str;
  }

  /// Returns the scheme component.
  string_view scheme() const noexcept {
    return impl_->scheme;
  }

  /// Returns the authority component.
  const authority_type& authority() const noexcept {
    return impl_->authority;
  }

  /// Returns the path component as provided by the user.
  string_view path() const noexcept {
    return impl_->path;
  }

  /// Returns the query component as key-value map.
  const query_map& query() const noexcept {
    return impl_->query;
  }

  /// Returns the fragment component.
  string_view fragment() const noexcept {
    return impl_->fragment;
  }

  /// Returns a hash code over all components.
  size_t hash_code() const noexcept;

  /// Returns a new URI with the `authority` component only.
  /// @returns A new URI in the form `scheme://authority` if the authority
  ///          exists, otherwise `none`.`
  std::optional<uri> authority_only() const;

  // -- comparison -------------------------------------------------------------

  auto compare(const uri& other) const noexcept {
    return str().compare(other.str());
  }

  auto compare(string_view x) const noexcept {
    return str().compare(x);
  }

  // -- parsing ----------------------------------------------------------------

  /// Returns whether `parse` would produce a valid URI.
  static bool can_parse(string_view str) noexcept;

private:
  impl_ptr impl_;
};

// -- related free functions ---------------------------------------------------

template <class Inspector>
bool inspect(Inspector& f, uri::authority_type& x) {
  return f.object(x).fields(f.field("userinfo", x.userinfo),
                            f.field("host", x.host), f.field("port", x.port));
}

template <class Inspector>
bool inspect(Inspector& f, uri::impl_type& x) {
  auto load_cb = [&] {
    x.assemble_str();
    return true;
  };
  return f.object(x)
    .on_load(load_cb) //
    .fields(f.field("str", x.str), f.field("scheme", x.scheme),
            f.field("authority", x.authority), f.field("path", x.path),
            f.field("query", x.query), f.field("fragment", x.fragment));
}

/// @relates uri
CAF_CORE_EXPORT std::string to_string(const uri& x);

/// @relates uri
CAF_CORE_EXPORT std::string to_string(const uri::authority_type& x);

/// @relates uri
CAF_CORE_EXPORT error parse(string_view str, uri& dest);

/// @relates uri
CAF_CORE_EXPORT expected<uri> make_uri(string_view str);

template <>
struct inspector_access<uri> : inspector_access_base<uri> {
  template <class Inspector>
  static bool apply(Inspector& f, uri& x) {
    if (f.has_human_readable_format()) {
      auto get = [&x] { return to_string(x); };
      auto set = [&x](std::string str) {
        auto err = parse(str, x);
        return !err;
      };
      return f.apply(get, set);
    } else {
      if constexpr (Inspector::is_loading)
        if (!x.impl_->unique())
          x.impl_.reset(new uri::impl_type, false);
      return inspect(f, *x.impl_);
    }
  }
};

} // namespace caf

namespace std {

template <>
struct hash<caf::uri> {
  size_t operator()(const caf::uri& x) const noexcept {
    return caf::hash::fnv<size_t>::compute(x);
  }
};

} // namespace std
