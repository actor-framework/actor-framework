// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_control_block.hpp"
#include "caf/async/batch.hpp"
#include "caf/cow_string.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/stream.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace caf {

/// Provides access to a statically typed, potentially unbound sequence of items
/// emitted by an actor. Each stream is uniquely identified by the address of
/// the hosting actor plus an integer value. Further, streams have
/// human-readable names attached to them in order to make help with
/// observability and logging.
template <class T>
class typed_stream : private detail::comparable<typed_stream<T>>,
                     private detail::comparable<stream> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  typed_stream() = default;

  typed_stream(typed_stream&&) noexcept = default;

  typed_stream(const typed_stream&) noexcept = default;

  typed_stream& operator=(typed_stream&&) noexcept = default;

  typed_stream& operator=(const typed_stream&) noexcept = default;

  typed_stream(strong_actor_ptr source, std::string name, uint64_t id)
    : source_(std::move(source)), name_(std::move(name)), id_(id) {
    // nop
  }

  typed_stream(strong_actor_ptr source, cow_string name, uint64_t id)
    : source_(std::move(source)), name_(std::move(name)), id_(id) {
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

  // -- conversion -------------------------------------------------------------

  /// Returns a dynamically typed version of this stream.
  stream dynamically_typed() const noexcept {
    return {source_, type_id_v<T>, name_, id_};
  }

  // -- comparison -------------------------------------------------------------

  ptrdiff_t compare(const stream& other) const noexcept {
    return compare_impl(other);
  }

  ptrdiff_t compare(const typed_stream& other) const noexcept {
    return compare_impl(other);
  }

  // -- serialization ----------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, typed_stream& obj) {
    return f.object(obj).fields(f.field("source", obj.source_),
                                f.field("name", obj.name_),
                                f.field("id", obj.id_));
  }

private:
  template <class OtherStream>
  ptrdiff_t compare_impl(const OtherStream& other) const noexcept {
    if (source_ < other.source())
      return -1;
    if (source_ == other.source()) {
      if (id_ < other.id())
        return -1;
      if (id_ == other.id())
        return 0;
    }
    return 1;
  }

  strong_actor_ptr source_;
  cow_string name_;
  uint64_t id_ = 0;
};

} // namespace caf
