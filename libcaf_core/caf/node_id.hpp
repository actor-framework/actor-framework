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

#include <array>
#include <string>
#include <cstdint>
#include <functional>

#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/error.hpp"
#include "caf/config.hpp"
#include "caf/ref_counted.hpp"
#include "caf/make_counted.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/meta/type_name.hpp"
#include "caf/meta/hex_formatted.hpp"

#include "caf/detail/comparable.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// A node ID consists of a host ID and process ID. The host ID identifies
/// the physical machine in the network, whereas the process ID identifies
/// the running system-level process on that machine.
class node_id {
public:
  ~node_id();

  node_id() = default;

  node_id(const node_id&) = default;

  node_id(const none_t&);

  node_id& operator=(const node_id&) = default;

  node_id& operator=(const none_t&);

  /// A 160 bit hash (20 bytes).
  static constexpr size_t host_id_size = 20;

  /// The size of a `node_id` in serialized form.
  static constexpr size_t serialized_size = host_id_size + sizeof(uint32_t);

  /// Represents a 160 bit hash.
  using host_id_type = std::array<uint8_t, host_id_size>;

  /// Creates a node ID from `process_id` and `hash`.
  /// @param procid System-wide unique process identifier.
  /// @param hash Unique node id as hexadecimal string representation.
  node_id(uint32_t procid, const std::string& hash);

  /// Creates a node ID from `process_id` and `hash`.
  /// @param procid System-wide unique process identifier.
  /// @param hid Unique node id.
  node_id(uint32_t procid, const host_id_type& hid);

  /// Identifies the running process.
  /// @returns A system-wide unique process identifier.
  uint32_t process_id() const;

  /// Identifies the host system.
  /// @returns A hash build from the MAC address of the first network device
  ///      and the UUID of the root partition (mounted in "/" or "C:").
  const host_id_type& host_id() const;

  /// Queries whether this node is not default-constructed.
  explicit operator bool() const;

  void swap(node_id&);

  /// @cond PRIVATE

  // A reference counted container for host ID and process ID.
  class data : public ref_counted {
  public:
    static intrusive_ptr<data> create_singleton();

    int compare(const node_id& other) const;

    ~data() override;

    data();

    data(uint32_t procid, host_id_type hid);

    data(uint32_t procid, const std::string& hash);

    data(const data&) = default;

    data& operator=(const data&) = default;

    bool valid() const;

    uint32_t pid_;

    host_id_type host_;
  };

  // "inherited" from comparable<node_id>
  int compare(const node_id& other) const;

  // "inherited" from comparable<node_id, invalid_node_id_t>
  int compare(const none_t&) const;

  explicit node_id(intrusive_ptr<data> dataptr);

  template <class Inspector>
  friend detail::enable_if_t<Inspector::reads_state,
                             typename Inspector::result_type>
  inspect(Inspector& f, node_id& x) {
    data tmp;
    data* ptr = x ? x.data_.get() : &tmp;
    return f(meta::type_name("node_id"), ptr->pid_,
             meta::hex_formatted(), ptr->host_);
  }

  template <class Inspector>
  friend detail::enable_if_t<Inspector::writes_state,
                             typename Inspector::result_type>
  inspect(Inspector& f, node_id& x) {
    data tmp;
    // write changes to tmp back to x at scope exit
    auto sg = detail::make_scope_guard([&] {
      if (!tmp.valid())
        x.data_.reset();
      else if (!x || !x.data_->unique())
        x.data_ = make_counted<data>(tmp);
      else
        *x.data_ = tmp;
    });
    return f(meta::type_name("node_id"), tmp.pid_,
             meta::hex_formatted(), tmp.host_);
  }

  /// @endcond

private:
  intrusive_ptr<data> data_;
};

/// @relates node_id
inline bool operator==(const node_id& x, none_t) {
  return !x;
}

/// @relates node_id
inline bool operator==(none_t, const node_id& x) {
  return !x;
}

/// @relates node_id
inline bool operator!=(const node_id& x, none_t) {
  return static_cast<bool>(x);
}

/// @relates node_id
inline bool operator!=(none_t, const node_id& x) {
  return static_cast<bool>(x);
}

inline bool operator==(const node_id& lhs, const node_id& rhs) {
  return lhs.compare(rhs) == 0;
}

inline bool operator!=(const node_id& lhs, const node_id& rhs) {
  return !(lhs == rhs);
}

inline bool operator<(const node_id& lhs, const node_id& rhs) {
  return lhs.compare(rhs) < 0;
}

/// Converts `x` into a human-readable string representation.
/// @relates node_id
std::string to_string(const node_id& x);

/// Appends `y` in human-readable string representation to `x`.
/// @relates node_id
void append_to_string(std::string& x, const node_id& y);

} // namespace caf

namespace std{

template<>
struct hash<caf::node_id> {
  size_t operator()(const caf::node_id& nid) const {
    if (nid == caf::none)
      return 0;
    // xor the first few bytes from the node ID and the process ID
    auto x = static_cast<size_t>(nid.process_id());
    auto y = *(reinterpret_cast<const size_t*>(&nid.host_id()));
    return x ^ y;
  }
};

} // namespace std

