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

#ifndef CAF_GROUP_HPP
#define CAF_GROUP_HPP

#include <string>

#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/abstract_group.hpp"

#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

class channel;
class message;

struct invalid_group_t {
  constexpr invalid_group_t() {}
};

/// Identifies an invalid {@link group}.
/// @relates group
constexpr invalid_group_t invalid_group = invalid_group_t{};

class group : detail::comparable<group>,
              detail::comparable<group, invalid_group_t> {
public:
  template <class T, typename U>
  friend T actor_cast(const U&);

  group() = default;

  group(group&&) = default;

  group(const group&) = default;

  group(const invalid_group_t&);

  group& operator=(group&&) = default;

  group& operator=(const group&) = default;

  group& operator=(const invalid_group_t&);

  group(abstract_group*);

  group(intrusive_ptr<abstract_group> ptr);

  inline explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  inline bool operator!() const noexcept {
    return ! ptr_;
  }

  /// Returns a handle that grants access to actor operations such as enqueue.
  inline abstract_group* operator->() const noexcept {
    return get();
  }

  inline abstract_group& operator*() const noexcept {
    return *get();
  }

  static intptr_t compare(const abstract_group* lhs, const abstract_group* rhs);

  intptr_t compare(const group& other) const noexcept;

  inline intptr_t compare(const invalid_group_t&) const noexcept {
    return ptr_ ? 1 : 0;
  }

  friend void serialize(serializer& sink, const group& x, const unsigned int);

  friend void serialize(deserializer& sink, group& x, const unsigned int);

private:
  inline abstract_group* get() const noexcept {
    return ptr_.get();
  }

  abstract_group_ptr ptr_;
};

/// @relates group
std::string to_string(const group& x);

} // namespace caf

#endif // CAF_GROUP_HPP
