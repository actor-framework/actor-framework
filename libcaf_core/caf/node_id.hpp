// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/hash/fnv.hpp"
#include "caf/inspector_access.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/none.hpp"
#include "caf/ref_counted.hpp"
#include "caf/uri.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>

namespace caf {

class CAF_CORE_EXPORT hashed_node_id {
public:
  // -- member types -----------------------------------------------------------

  /// Represents a 160 bit hash.
  using host_id_type = std::array<uint8_t, 20>;

  // -- constructors, destructors, and assignment operators --------------------

  hashed_node_id() noexcept;

  hashed_node_id(uint32_t process_id, const host_id_type& host) noexcept;

  // -- properties -------------------------------------------------------------

  bool valid() const noexcept;

  // -- comparison -------------------------------------------------------------

  int compare(const hashed_node_id& other) const noexcept;

  // -- conversion -------------------------------------------------------------

  void print(std::string& dst) const;

  // -- static utility functions -----------------------------------------------

  static bool valid(const host_id_type& x) noexcept;

  static bool can_parse(std::string_view str) noexcept;

  static node_id local(const actor_system_config&);

  // -- member variables -------------------------------------------------------

  uint32_t process_id;

  host_id_type host;

  // -- friend functions -------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, hashed_node_id& x) {
    return f.object(x).fields(f.field("process_id", x.process_id),
                              f.field("host", x.host));
  }
};

class CAF_CORE_EXPORT node_id_data : public ref_counted {
public:
  // -- member types -----------------------------------------------------------

  using variant_type = std::variant<uri, hashed_node_id>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit node_id_data(variant_type value) : content(std::move(value)) {
    // nop
  }

  explicit node_id_data(uri value) : content(std::move(value)) {
    // nop
  }

  explicit node_id_data(const hashed_node_id& value)
    : content(std::move(value)) {
    // nop
  }

  node_id_data() = default;

  node_id_data(node_id_data&&) = default;

  node_id_data(const node_id_data&) = default;

  node_id_data& operator=(node_id_data&&) = default;

  node_id_data& operator=(const node_id_data&) = default;

  virtual ~node_id_data();

  // -- member variables -------------------------------------------------------

  variant_type content;
};

/// A node ID is an opaque value for representing CAF instances in the network.
class CAF_CORE_EXPORT node_id {
public:
  // -- member types -----------------------------------------------------------

  using default_data = hashed_node_id;

  // -- constructors, destructors, and assignment operators --------------------

  constexpr node_id() noexcept {
    // nop
  }

  explicit node_id(hashed_node_id data) {
    if (data.valid())
      data_.emplace(data);
  }

  explicit node_id(uri data) {
    if (data.valid())
      data_.emplace(std::move(data));
  }

  node_id& operator=(const none_t&);

  // -- properties -------------------------------------------------------------

  /// Queries whether this node is not default-constructed.
  explicit operator bool() const noexcept {
    return static_cast<bool>(data_);
  }

  /// Queries whether this node is default-constructed.
  bool operator!() const noexcept {
    return !data_;
  }

  /// Compares this instance to `other`.
  /// @returns -1 if `*this < other`, 0 if `*this == other`, and 1 otherwise.
  int compare(const node_id& other) const noexcept;

  /// Exchanges the value of this object with `other`.
  void swap(node_id& other) noexcept;

  /// Returns whether `parse` would produce a valid node ID.
  static bool can_parse(std::string_view str) noexcept;

  // -- friend functions -------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, node_id& x) {
    auto is_present = [&x] { return x.data_ != nullptr; };
    auto get = [&]() -> const auto& { return x.data_->content; };
    auto reset = [&x] { x.data_.reset(); };
    auto set = [&x](node_id_data::variant_type&& val) {
      if (x.data_ && x.data_->unique())
        x.data_->content = std::move(val);
      else
        x.data_.emplace(std::move(val));
      return true;
    };
    return f.object(x).fields(f.field("data", is_present, get, reset, set));
  }

  // -- private API ------------------------------------------------------------

  /// @cond

  auto* operator->() noexcept {
    return data_.get();
  }

  const auto* operator->() const noexcept {
    return data_.get();
  }

  auto& operator*() noexcept {
    return *data_;
  }

  const auto& operator*() const noexcept {
    return *data_;
  }

  /// @endcond

private:
  intrusive_ptr<node_id_data> data_;
};

/// Returns whether `x` contains an URI.
/// @relates node_id
inline bool wraps_uri(const node_id& x) noexcept {
  return x && std::holds_alternative<uri>(x->content);
}

/// @relates node_id
inline bool operator==(const node_id& x, const node_id& y) noexcept {
  return x.compare(y) == 0;
}

/// @relates node_id
inline bool operator!=(const node_id& x, const node_id& y) noexcept {
  return x.compare(y) != 0;
}

/// @relates node_id
inline bool operator<(const node_id& x, const node_id& y) noexcept {
  return x.compare(y) < 0;
}

/// @relates node_id
inline bool operator<=(const node_id& x, const node_id& y) noexcept {
  return x.compare(y) <= 0;
}

/// @relates node_id
inline bool operator>(const node_id& x, const node_id& y) noexcept {
  return x.compare(y) > 0;
}

/// @relates node_id
inline bool operator>=(const node_id& x, const node_id& y) noexcept {
  return x.compare(y) >= 0;
}

/// @relates node_id
inline bool operator==(const node_id& x, const none_t&) noexcept {
  return !x;
}

/// @relates node_id
inline bool operator==(const none_t&, const node_id& x) noexcept {
  return !x;
}

/// @relates node_id
inline bool operator!=(const node_id& x, const none_t&) noexcept {
  return static_cast<bool>(x);
}

/// @relates node_id
inline bool operator!=(const none_t&, const node_id& x) noexcept {
  return static_cast<bool>(x);
}

/// Appends `x` in human-readable string representation to `str`.
/// @relates node_id
CAF_CORE_EXPORT void append_to_string(std::string& str, const node_id& x);

/// Converts `x` into a human-readable string representation.
/// @relates node_id
CAF_CORE_EXPORT std::string to_string(const node_id& x);

/// Creates a node ID from the URI `from`.
/// @relates node_id
CAF_CORE_EXPORT node_id make_node_id(uri from);

/// Creates a node ID from `process_id` and `host_id`.
/// @param process_id System-wide unique process identifier.
/// @param host_id Unique hash value representing a single CAF node.
/// @relates node_id
CAF_CORE_EXPORT node_id make_node_id(
  uint32_t process_id, const node_id::default_data::host_id_type& host_id);

/// Creates a node ID from `process_id` and `host_hash`.
/// @param process_id System-wide unique process identifier.
/// @param host_hash Unique node ID as hexadecimal string representation.
/// @relates node_id
CAF_CORE_EXPORT std::optional<node_id> make_node_id(uint32_t process_id,
                                                    std::string_view host_hash);

/// @relates node_id
CAF_CORE_EXPORT error parse(std::string_view str, node_id& dest);

} // namespace caf

namespace std {

template <>
struct hash<caf::node_id> {
  size_t operator()(const caf::node_id& x) const noexcept {
    return caf::hash::fnv<size_t>::compute(x);
  }
};

} // namespace std
