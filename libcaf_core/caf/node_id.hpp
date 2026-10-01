// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/append_hex.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/formatted.hpp"
#include "caf/detail/print.hpp"
#include "caf/fwd.hpp"
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

  [[nodiscard]] size_t hash() const noexcept;

  // -- comparison -------------------------------------------------------------

  constexpr auto operator<=>(const hashed_node_id&) const noexcept = default;

  // -- conversion -------------------------------------------------------------

  void print(std::string& dst) const;

  // -- static utility functions -----------------------------------------------

  static bool valid(const host_id_type& x) noexcept;

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

  node_id_data() = default;

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

  ~node_id_data() noexcept override;

  bool operator==(const node_id_data& other) const noexcept {
    return content == other.content;
  }

  auto operator<=>(const node_id_data& other) const noexcept {
    return content <=> other.content;
  }

  variant_type content;
};

} // namespace caf

namespace caf::detail {

template <class>
struct node_id_index_impl;

template <>
struct node_id_index_impl<none_t> {
  static constexpr size_t value = 0;
};

template <>
struct node_id_index_impl<uri> {
  static constexpr size_t value = 1;
};

template <>
struct node_id_index_impl<hashed_node_id> {
  static constexpr size_t value = 2;
};

template <class T>
constexpr size_t node_id_index = node_id_index_impl<T>::value;

} // namespace caf::detail

namespace caf {

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

private:
  template <class Visitor>
  auto visit(Visitor&& visitor) const noexcept {
    if (data_) {
      return std::visit(std::forward<Visitor>(visitor), data_->content);
    }
    return std::forward<Visitor>(visitor)(none);
  }

public:
  bool operator==(const node_id& other) const noexcept {
    return visit([&other]<class Left>(const Left& lhs) {
      return other.visit([&lhs]<class Right>(const Right& rhs) {
        if constexpr (std::is_same_v<Left, Right>) {
          return lhs == rhs;
        } else {
          return false;
        }
      });
    });
  }

  auto operator<=>(const node_id& other) const noexcept {
    return visit([&other]<class Left>(const Left& lhs) {
      return other.visit([&lhs]<class Right>(const Right& rhs) {
        if constexpr (std::is_same_v<Left, Right>) {
          return lhs <=> rhs;
        } else {
          return detail::node_id_index<Left> <=> detail::node_id_index<Right>;
        }
      });
    });
  }

  /// Exchanges the value of this object with `other`.
  void swap(node_id& other) noexcept;

  [[nodiscard]] size_t hash() const noexcept;

  // -- friend functions -------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, node_id& x) {
    auto is_present = [&x] { return x.data_ != nullptr; };
    auto getter = [&]() -> const auto& { return x.data_->content; };
    auto reset = [&x] { x.data_.reset(); };
    auto set = [&x](node_id_data::variant_type&& val) {
      if (x.data_ && x.data_->strong_reference_count() == 1)
        x.data_->content = std::move(val);
      else
        x.data_.emplace(std::move(val));
      return true;
    };
    return f.object(x).fields(f.field("data", is_present, getter, reset, set));
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
inline bool operator==(const node_id& x, const none_t&) noexcept {
  return !x;
}

/// @relates node_id
inline bool operator==(const none_t&, const node_id& x) noexcept {
  return !x;
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

} // namespace caf

namespace std {

template <>
struct hash<caf::node_id> {
  size_t operator()(const caf::node_id& x) const noexcept {
    return x.hash();
  }
};

} // namespace std

namespace caf::detail {

template <>
struct simple_formatter<node_id> {
  template <class OutputIterator>
  OutputIterator format(const caf::node_id& nid, OutputIterator out) const {
    using namespace std::literals;
    if (!nid) {
      const auto prefix = "caf:local"sv;
      print_iterator_adapter<OutputIterator> buf{out};
      buf.insert(buf.end(), prefix.begin(), prefix.end());
      return buf.pos;
    }
    const auto& content = nid->content;
    if (std::holds_alternative<uri>(content)) {
      const auto str = std::get<uri>(content).str();
      return std::copy(str.begin(), str.end(), out);
    }
    const auto& hashed = std::get<hashed_node_id>(content);
    const auto prefix = "caf:io:"sv;
    print_iterator_adapter<OutputIterator> buf{out};
    buf.insert(buf.end(), prefix.begin(), prefix.end());
    append_hex<hex_format::lowercase>(buf, hashed.host.data(),
                                      hashed.host.size());
    buf.push_back(':');
    print(buf, hashed.process_id);
    return buf.pos;
  }
};

} // namespace caf::detail
