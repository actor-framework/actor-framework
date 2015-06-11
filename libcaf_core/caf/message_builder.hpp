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

#ifndef CAF_MESSAGE_BUILDER_HPP
#define CAF_MESSAGE_BUILDER_HPP

#include <vector>

#include "caf/message.hpp"
#include "caf/message_handler.hpp"
#include "caf/uniform_type_info.hpp"

namespace caf {

/// Provides a convenient interface for createing `message` objects
/// from a series of values using the member function `append`.
class message_builder {
public:
  message_builder(const message_builder&) = delete;
  message_builder& operator=(const message_builder&) = delete;

  message_builder();
  ~message_builder();

  /// Creates a new instance and immediately calls `append(first, last)`.
  template <class Iter>
  message_builder(Iter first, Iter last) {
    init();
    append(first, last);
  }

  /// Adds `what` to the elements of the buffer.
  message_builder& append(uniform_value what);

  /// Appends all values in range [first, last).
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

  /// Adds `x` to the elements of the buffer.
  template <class T>
  message_builder& append(T x) {
    return append_impl<T>(std::move(x));
  }

  /// Converts the buffer to an actual message object without
  /// invalidating this message builder (nor clearing it).
  message to_message() const;

  /// Converts the buffer to an actual message object and transfers
  /// ownership of the data to it, leaving this object in an invalid state.
  /// @warning Calling *any*  member function on this object afterwards
  ///          is undefined behavior (dereferencing a `nullptr`)
  message move_to_message();

  /// @copydoc message::extract
  inline message extract(message_handler f) const {
    return to_message().extract(f);
  }

  /// @copydoc message::extract_opts
  inline message::cli_res extract_opts(std::vector<message::cli_arg> xs,
                                       message::help_factory f
                                       = nullptr) const {
    return to_message().extract_opts(std::move(xs), std::move(f));
  }

  /// @copydoc message::apply
  optional<message> apply(message_handler handler);

  /// Removes all elements from the buffer.
  void clear();

  /// Returns whether the buffer is empty.
  bool empty() const;

  /// Returns the number of elements in the buffer.
  size_t size() const;

private:
  void init();

  template <class T>
  message_builder&
  append_impl(typename unbox_message_element<
                typename detail::implicit_conversions<T>::type
              >::type what) {
    using type = decltype(what);
    auto uti = uniform_typeid<type>();
    auto uval = uti->create();
    *reinterpret_cast<type*>(uval->val) = std::move(what);
    return append(std::move(uval));
  }

  class dynamic_msg_data;

  dynamic_msg_data* data();

  const dynamic_msg_data* data() const;

  intrusive_ptr<ref_counted> data_; // hide dynamic_msg_data implementation
};

} // namespace caf

#endif // CAF_MESSAGE_BUILDER_HPP
