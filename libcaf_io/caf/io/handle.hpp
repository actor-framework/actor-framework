/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_HANDLE_HPP
#define CAF_DETAIL_HANDLE_HPP

#include <cstdint>

#include "caf/detail/comparable.hpp"

namespace caf {

/// Base class for IO handles such as `accept_handle` or `connection_handle`.
template <class Subtype, int64_t InvalidId = -1>
class handle : detail::comparable<Subtype> {
public:
  constexpr handle() : id_{InvalidId} {
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

  bool invalid() const {
    return id_ == InvalidId;
  }

  void set_invalid() {
    set_id(InvalidId);
  }

  static Subtype from_int(int64_t id) {
    return {id};
  }

protected:
  handle(int64_t handle_id) : id_{handle_id} {
    // nop
  }

private:
  int64_t id_;
};

} // namespace caf

#endif // CAF_DETAIL_HANDLE_HPP
