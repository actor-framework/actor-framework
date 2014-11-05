/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_OBJECT_ARRAY_HPP
#define CAF_OBJECT_ARRAY_HPP

#include <vector>

#include "caf/message.hpp"
#include "caf/message_handler.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/message_data.hpp"

namespace caf {

/**
 * Provides a convenient interface to create a {@link message}
 * from a series of values using the member function `append`.
 */
class message_builder {
 public:
  message_builder();
  message_builder(const message_builder&) = delete;
  message_builder& operator=(const message_builder&) = delete;
  ~message_builder();

  /**
   * Creates a new instance and immediately calls `append(first, last)`.
   */
  template <class Iter>
  message_builder(Iter first, Iter last) {
    init();
    append(first, last);
  }

  /**
   * Adds `what` to the elements of the internal buffer.
   */
  message_builder& append(uniform_value what);

  /**
   * Appends all values in range [first, last).
   */
  template <class Iter>
  message_builder& append(Iter first, Iter last) {
    using vtype = typename std::decay<decltype(*first)>::type;
    using converted = typename detail::implicit_conversions<vtype>::type;
    auto uti = uniform_typeid<converted>();
    for (; first != last; ++first) {
      auto uval = uti->create();
      *reinterpret_cast<converted*>(uval->val) = *first;
      append(std::move(uval));
    }
    return *this;
  }

  /**
   * Adds `what` to the elements of the internal buffer.
   */
  template <class T>
  message_builder& append(T what) {
    return append_impl<T>(std::move(what));
  }

  /**
   * Converts the internal buffer to an actual message object.
   *
   * It is worth mentioning that a call to `to_message` does neither
   * invalidate the `message_builder` instance nor clears the internal
   * buffer. However, calling any non-const member function afterwards
   * can cause the `message_builder` to detach its data, i.e.,
   * copy it if there is more than one reference to it.
   */
  message to_message();

  /**
   * Convenience function for `to_message().apply(handler).
   */
  inline optional<message> apply(message_handler handler) {
    return to_message().apply(std::move(handler));
  }

  /**
   * Removes all elements from the internal buffer.
   */
  void clear();

  /**
   * Returns whether the internal buffer is empty.
   */
  bool empty() const;

  /**
   * Returns the number of elements in the internal buffer.
   */
  size_t size() const;

 private:
  void init();

  template <class T>
  message_builder&
  append_impl(typename detail::implicit_conversions<T>::type what) {
    using type = decltype(what);
    auto uti = uniform_typeid<type>();
    auto uval = uti->create();
    *reinterpret_cast<type*>(uval->val) = std::move(what);
    return append(std::move(uval));
  }

  class dynamic_msg_data;

  dynamic_msg_data* data();

  const dynamic_msg_data* data() const;

  intrusive_ptr<ref_counted> m_data; // hide dynamic_msg_data implementation
};

} // namespace caf

#endif // CAF_OBJECT_ARRAY_HPP
