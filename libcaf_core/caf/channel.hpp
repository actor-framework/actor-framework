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

#ifndef CAF_CHANNEL_HPP
#define CAF_CHANNEL_HPP

#include <cstddef>
#include <type_traits>

#include "caf/intrusive_ptr.hpp"

#include "caf/fwd.hpp"
#include "caf/abstract_channel.hpp"

#include "caf/detail/comparable.hpp"

namespace caf {

class actor;
class group;
class execution_unit;

struct invalid_actor_t;
struct invalid_group_t;

/// A handle to instances of `abstract_channel`.
class channel : detail::comparable<channel>,
                detail::comparable<channel, actor>,
                detail::comparable<channel, abstract_channel*> {
public:
  template <class T, typename U>
  friend T actor_cast(const U&);

  channel() = default;

  channel(const actor&);

  channel(const group&);

  channel(const invalid_actor_t&);

  channel(const invalid_group_t&);

  template <class T>
  channel(intrusive_ptr<T> ptr,
          typename std::enable_if<
            std::is_base_of<abstract_channel, T>::value
          >::type* = 0)
      : ptr_(ptr) {
    // nop
  }

  channel(abstract_channel* ptr);

  inline explicit operator bool() const {
    return static_cast<bool>(ptr_);
  }

  inline bool operator!() const {
    return ! ptr_;
  }

  inline abstract_channel* operator->() const {
    return ptr_.get();
  }

  inline abstract_channel& operator*() const {
    return *ptr_;
  }

  intptr_t compare(const channel& other) const;

  intptr_t compare(const actor& other) const;

  intptr_t compare(const abstract_channel* other) const;

  static intptr_t compare(const abstract_channel* lhs,
                          const abstract_channel* rhs);

private:
  inline abstract_channel* get() const {
    return ptr_.get();
  }

  abstract_channel_ptr ptr_;
};

} // namespace caf

#endif // CAF_CHANNEL_HPP
