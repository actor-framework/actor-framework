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


#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/abstract_channel.hpp"

#include "caf/detail/comparable.hpp"

namespace caf {

struct invalid_channel_t {
  constexpr invalid_channel_t() {
    // nop
  }
};

/// Identifies an invalid {@link channel}.
/// @relates channel
constexpr invalid_channel_t invalid_channel = invalid_channel_t{};


/// A handle to instances of `abstract_channel`.
class channel : detail::comparable<channel>,
                detail::comparable<channel, actor>,
                detail::comparable<channel, abstract_channel*> {
public:
  template <class>
  friend struct actor_cast_access;

  channel() = default;
  channel(channel&&) = default;
  channel(const channel&) = default;
  channel& operator=(channel&&) = default;
  channel& operator=(const channel&) = default;

  channel(const actor&);
  channel(const group&);
  channel(const scoped_actor&);
  channel(const invalid_channel_t&);

  template <class T>
  channel(intrusive_ptr<T> ptr,
          typename std::enable_if<
            std::is_base_of<abstract_channel, T>::value
          >::type* = 0)
      : ptr_(ptr) {
    // nop
  }

  /// Allows actors to create a `channel` handle from `this`.
  channel(local_actor*);

  channel& operator=(const actor&);
  channel& operator=(const group&);
  channel& operator=(const scoped_actor&);
  channel& operator=(const invalid_channel_t&);

  inline explicit operator bool() const noexcept {
    return static_cast<bool>(ptr_);
  }

  inline bool operator!() const noexcept {
    return ! ptr_;
  }

  inline abstract_channel* operator->() const noexcept {
    return get();
  }

  inline abstract_channel& operator*() const noexcept {
    return *ptr_;
  }

  intptr_t compare(const channel& other) const noexcept;

  intptr_t compare(const actor& other) const noexcept;

  intptr_t compare(const abstract_channel* other) const noexcept;

  /// @relates channel
  friend void serialize(serializer& sink, channel& x, const unsigned int);

  /// @relates channel
  friend void serialize(deserializer& source, channel& x, const unsigned int);

private:
  channel(abstract_channel*);

  channel(abstract_channel*, bool);

  inline abstract_channel* get() const noexcept {
    return ptr_.get();
  }

  abstract_channel_ptr ptr_;
};

/// @relates channel
std::string to_string(const channel& x);

} // namespace caf

#endif // CAF_CHANNEL_HPP
