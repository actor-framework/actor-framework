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

#include "caf/intrusive_ptr.hpp"

#include "caf/fwd.hpp"
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

  template <class T, typename U>
  friend T actor_cast(const U&);

public:

  group() = default;

  group(group&&) = default;

  group(const group&) = default;

  group(const invalid_group_t&);

  group& operator=(group&&) = default;

  group& operator=(const group&) = default;

  group& operator=(const invalid_group_t&);

  group(intrusive_ptr<abstract_group> ptr);

  inline explicit operator bool() const { return static_cast<bool>(ptr_); }

  inline bool operator!() const { return ! static_cast<bool>(ptr_); }

  /// Returns a handle that grants access to actor operations such as enqueue.
  inline abstract_group* operator->() const { return ptr_.get(); }

  inline abstract_group& operator*() const { return *ptr_; }

  intptr_t compare(const group& other) const;

  inline intptr_t compare(const invalid_actor_t&) const {
    return ptr_ ? 1 : 0;
  }

  /// Get a pointer to the group associated with
  /// `identifier` from the module `mod_name`.
  /// @threadsafe
  static group get(const std::string& mod_name, const std::string& identifier);

  /// Returns an anonymous group.
  /// Each calls to this member function returns a new instance
  /// of an anonymous group. Anonymous groups can be used whenever
  /// a set of actors wants to communicate using an exclusive channel.
  static group anonymous();

  /// Add a new group module to the group management.
  /// @threadsafe
  static void add_module(abstract_group::unique_module_ptr);

  /// Returns the module associated with `module_name`.
  /// @threadsafe
  static abstract_group::module_ptr
  get_module(const std::string& module_name);

  inline const abstract_group_ptr& ptr() const {
    return ptr_;
  }

private:
  abstract_group_ptr ptr_;
};

} // namespace caf

#endif // CAF_GROUP_HPP
