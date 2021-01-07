// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>
#include <cstdint>

#include "caf/detail/comparable.hpp"

namespace caf {

/// Base class for IO handles such as `accept_handle` or `connection_handle`.
template <class Subtype, class InvalidType, int64_t InvalidId = -1>
class handle : detail::comparable<Subtype>,
               detail::comparable<Subtype, InvalidType> {
public:
  constexpr handle() : id_(InvalidId) {
    // nop
  }

  handle(const Subtype& other) {
    id_ = other.id();
  }

  handle(const handle& other) = default;

  handle& operator=(const handle& other) {
    id_ = other.id();
    return *this;
  }

  handle& operator=(const InvalidType&) {
    id_ = InvalidId;
    return *this;
  }

  /// Returns the unique identifier of this handle.
  int64_t id() const {
    return id_;
  }

  /// Sets the unique identifier of this handle.
  void set_id(int64_t value) {
    id_ = value;
  }

  int64_t compare(const Subtype& other) const {
    return id_ - other.id();
  }

  int64_t compare(const InvalidType&) const {
    return invalid() ? 0 : 1;
  }

  bool invalid() const {
    return id_ == InvalidId;
  }

  void set_invalid() {
    set_id(InvalidId);
  }

  static Subtype from_int(int64_t id) {
    return {id};
  }

  friend std::string to_string(const Subtype& x) {
    return std::to_string(x.id());
  }

protected:
  handle(int64_t handle_id) : id_{handle_id} {
    // nop
  }

  int64_t id_;
};

} // namespace caf

