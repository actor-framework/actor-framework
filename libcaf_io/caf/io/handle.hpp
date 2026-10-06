// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"

#include <cstdint>
#include <string>

namespace caf::io {

/// Base class for IO handles such as `accept_handle` or `connection_handle`.
template <class Subtype, class InvalidType, int64_t InvalidId = -1>
class handle {
public:
  constexpr handle() noexcept = default;

  /// Returns the unique identifier of this handle.
  int64_t id() const noexcept {
    return id_;
  }

  /// Sets the unique identifier of this handle.
  void set_id(int64_t value) noexcept {
    id_ = value;
  }

  constexpr bool operator==(const handle&) const noexcept = default;

  constexpr auto operator<=>(const handle&) const noexcept = default;

  constexpr bool operator==(const InvalidType&) const noexcept {
    return id_ == InvalidId;
  }

  constexpr auto operator<=>(const InvalidType&) const noexcept {
    return id_ <=> InvalidId;
  }

  constexpr bool invalid() const noexcept {
    return id_ == InvalidId;
  }

  void set_invalid() {
    set_id(InvalidId);
  }

  static Subtype from_int(int64_t id) {
    return Subtype(id);
  }

  friend std::string to_string(const Subtype& x) {
    return std::to_string(x.id());
  }

protected:
  explicit handle(int64_t handle_id) : id_{handle_id} {
    // nop
  }

  int64_t id_ = InvalidId;
};

} // namespace caf::io
