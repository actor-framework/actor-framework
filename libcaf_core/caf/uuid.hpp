// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace caf {

/// A universally unique identifier according to
/// [RFC 4122](https://tools.ietf.org/html/rfc4122). While this implementation
/// can read all UUID versions, it can only create random-generated ones.
class CAF_CORE_EXPORT uuid : detail::comparable<uuid> {
public:
  using array_type = std::array<std::byte, 16>;

  /// Creates the nil UUID with all 128 bits set to zero.
  uuid() noexcept;

  uuid(const uuid&) noexcept = default;

  uuid& operator=(const uuid&) noexcept = default;

  explicit uuid(const array_type& bytes) noexcept : bytes_(bytes) {
    // nop
  }

  const array_type& bytes() const noexcept {
    return bytes_;
  }

  array_type& bytes() noexcept {
    return bytes_;
  }

  /// Returns `true` if this UUID is *not* `nil`, `false` otherwise.
  /// A UUID is `nil` if all bits are 0.
  operator bool() const noexcept;

  /// Returns `true` if this UUID is `nil`, `false` otherwise.
  bool operator!() const noexcept;

  /// Denotes the variant (type) that determines the layout of the UUID. The
  /// interpretation of all other bits in a UUID depend on this field.
  enum variant_field {
    reserved,
    rfc4122,
    microsoft,
  };

  /// Returns the variant (type) that determines the layout of the UUID.
  /// @pre `not nil()`
  variant_field variant() const noexcept;

  /// Denotes the version, i.e., which algorithm was used to create this UUID.
  enum version_field {
    /// Time-based algorithm.
    time_based = 1,

    /// DCE security version with embedded POSIX UIDs.
    dce_compatible = 2,

    /// Name-based algorithm using MD5 hashing.
    md5_based = 3,

    /// Random or pseudo-random algorithm.
    randomized = 4,

    /// Name-based algorithm using SHA-1 hashing.
    sha1_based = 5,
  };

  /// Returns the version (sub type) that identifies the algorithm used to
  /// generate this UUID. The algorithms defined in RFC 4122 are:
  ///
  /// 1. Time-based
  /// 2. DCE security with embedded POSIX UIDs
  /// 3. Name-based, using MD5 hashing
  /// 4. Random or pseudo-random
  /// 5. Name-based, using SHA-1 hashing
  /// @pre `not nil()`
  version_field version() const noexcept;

  /// The 60-bit timestamp of a time-based UUID. Usually represents a count of
  /// 100- nanosecond intervals since 00:00:00.00, 15 October 1582 in UTC.
  /// @pre `version() == time_based`
  uint64_t timestamp() const noexcept;

  /// The 14-bit unsigned integer helps to avoid duplicates that could arise
  /// when the clock is set backwards in time or if the node ID changes.
  /// @pre `version() == time_based`
  uint16_t clock_sequence() const noexcept;

  /// 48-bit value, representing a network address (`time_based` UUIDs), a hash
  /// (`md5_based` and `sha1_based` UUIDs), or a random bit sequence
  /// (`randomized` UUIDs).
  uint64_t node() const noexcept;

  /// Returns a platform-specific hash value for this UUID.
  size_t hash() const noexcept;

  /// Creates a random UUID.
  static uuid random() noexcept;

  /// Creates a random UUID with a predefined seed.
  static uuid random(unsigned seed) noexcept;

  /// Convenience function for creating an UUID with all 128 bits set to zero.
  static uuid nil() noexcept {
    return uuid{};
  }

  /// Returns whether `parse` would produce a valid UUID.
  static bool can_parse(std::string_view str) noexcept;

  /// Lexicographically compares `this` and `other`.
  /// @returns a negative value if `*this < other`, zero if `*this == other`
  ///          and a positive number if `*this > other`.
  int compare(const uuid& other) const noexcept {
    return memcmp(bytes_.data(), other.bytes_.data(), 16u);
  }

private:
  /// Stores the fields, encoded as 16 octets:
  ///
  /// ```
  /// 0                   1                   2                   3
  ///  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  /// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  /// |                          time_low                             |
  /// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  /// |       time_mid                |         time_hi_and_version   |
  /// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  /// |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
  /// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  /// |                         node (2-5)                            |
  /// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  /// ```
  array_type bytes_;
};

/// @relates uuid
CAF_CORE_EXPORT error parse(std::string_view str, uuid& dest);

/// @relates uuid
CAF_CORE_EXPORT std::string to_string(const uuid& x);

/// @relates uuid
CAF_CORE_EXPORT expected<uuid> make_uuid(std::string_view str);

/// @relates uuid
template <class Inspector>
bool inspect(Inspector& f, uuid& x) {
  if (f.has_human_readable_format()) {
    auto get = [&x] { return to_string(x); };
    auto set = [&x](std::string str) { return parse(str, x); };
    return f.apply(get, set);
  } else {
    return f.apply(x.bytes());
  }
}

} // namespace caf

namespace std {

template <>
struct hash<caf::uuid> {
  size_t operator()(const caf::uuid& x) const noexcept {
    return x.hash();
  }
};

} // namespace std
