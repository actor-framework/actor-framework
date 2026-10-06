// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/cow_string.hpp"
#include "caf/fwd.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>

namespace caf {

/// Provides access to a potentially unbound sequence of items emitted by an
/// actor. Each stream is uniquely identified by the address of the hosting
/// actor plus an integer value. Further, streams have human-readable names
/// attached to them in order to make help with observability and logging.
class stream {
public:
  // -- constructors, destructors, and assignment operators --------------------

  stream() noexcept = default;

  template <class Name>
    requires std::is_constructible_v<cow_string, Name>
  stream(strong_actor_ptr source, type_id_t type, Name&& name, uint64_t id = 0)
    : source_(std::move(source)),
      type_(type),
      name_(std::forward<Name>(name)),
      id_(id) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Checks whether this stream emits elements of type @c T.
  template <class T>
  bool has_element_type() const noexcept {
    return type_id_v<T> == type_;
  }

  /// Queries the source of this stream. Default-constructed streams return a
  /// @c null pointer.
  const strong_actor_ptr& source() const noexcept {
    return source_;
  }

  /// Returns the type ID of the items emitted by the source.
  type_id_t type() const noexcept {
    return type_;
  }

  /// Returns the human-readable name for this stream, as announced by the
  /// source.
  const std::string& name() const noexcept {
    return name_.str();
  }

  /// Returns the human-readable name for this stream, as announced by the
  /// source.
  const cow_string& cow_name() const noexcept {
    return name_;
  }

  /// Returns the source-specific identifier for this stream.
  uint64_t id() const noexcept {
    return id_;
  }

  /// Convenience function for wrapping the source and ID into a tuple.
  auto source_and_id() const noexcept {
    return std::tie(source_, id_);
  }

  // -- comparison -------------------------------------------------------------

  /// Returns whether this stream is equal to `other`.
  /// @note The comparison only considers the source and the ID.
  bool operator==(const stream& other) const noexcept {
    return source_and_id() == other.source_and_id();
  }

  /// Compares this stream to `other`.
  /// @note The comparison only considers the source and the ID.
  std::weak_ordering operator<=>(const stream& other) const noexcept {
    return source_and_id() <=> other.source_and_id();
  }

  // -- serialization ----------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, stream& obj) {
    return f.object(obj).fields(f.field("source", obj.source_),
                                f.field("type", obj.type_),
                                f.field("name", obj.name_),
                                f.field("id", obj.id_));
  }

private:
  strong_actor_ptr source_;
  type_id_t type_ = invalid_type_id;
  cow_string name_;
  uint64_t id_ = 0;
};

} // namespace caf
