// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/cow_string.hpp"
#include "caf/fwd.hpp"
#include "caf/stream.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>

namespace caf {

/// Provides access to a statically typed, potentially unbound sequence of items
/// emitted by an actor. Each stream is uniquely identified by the address of
/// the hosting actor plus an integer value. Further, streams have
/// human-readable names attached to them in order to make help with
/// observability and logging.
template <class T>
class typed_stream {
public:
  // -- constructors, destructors, and assignment operators --------------------

  typed_stream() noexcept = default;

  template <class Name>
    requires std::is_constructible_v<cow_string, Name>
  typed_stream(strong_actor_ptr source, Name&& name, uint64_t id = 0)
    : source_(std::move(source)), name_(std::forward<Name>(name)), id_(id) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  /// Queries the source of this stream. Default-constructed streams return a
  /// @c null pointer.
  const strong_actor_ptr& source() const noexcept {
    return source_;
  }

  /// Returns the human-readable name for this stream, as announced by the
  /// source.
  const std::string& name() const noexcept {
    return name_.str();
  }

  /// Returns the source-specific identifier for this stream.
  uint64_t id() const noexcept {
    return id_;
  }

  /// Convenience function for wrapping the source and ID into a tuple.
  auto source_and_id() const noexcept {
    return std::tie(source_, id_);
  }

  // -- conversion -------------------------------------------------------------

  /// Returns a dynamically typed version of this stream.
  stream dynamically_typed() const noexcept {
    return {source_, type_id_v<T>, name_, id_};
  }

  // -- comparison -------------------------------------------------------------

  /// Returns whether this stream is equal to `other`.
  /// @note The comparison only considers the source and the ID.
  bool operator==(const typed_stream& other) const noexcept {
    return source_and_id() == other.source_and_id();
  }

  /// Compares this stream to `other`.
  /// @note The comparison only considers the source and the ID.
  std::weak_ordering operator<=>(const typed_stream& other) const noexcept {
    return source_and_id() <=> other.source_and_id();
  }

  // -- serialization ----------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, typed_stream& obj) {
    return f.object(obj).fields(f.field("source", obj.source_),
                                f.field("name", obj.name_),
                                f.field("id", obj.id_));
  }

private:
  strong_actor_ptr source_;
  cow_string name_;
  uint64_t id_ = 0;
};

template <class T>
bool operator==(const stream& lhs, const typed_stream<T>& rhs) {
  return lhs.source_and_id() == rhs.source_and_id();
}

template <class T>
bool operator==(const typed_stream<T>& lhs, const stream& rhs) {
  return lhs.source_and_id() == rhs.source_and_id();
}

template <class T>
std::weak_ordering operator<=>(const stream& lhs, const typed_stream<T>& rhs) {
  return lhs.source_and_id() <=> rhs.source_and_id();
}

template <class T>
std::weak_ordering operator<=>(const typed_stream<T>& lhs, const stream& rhs) {
  return lhs.source_and_id() <=> rhs.source_and_id();
}

} // namespace caf
