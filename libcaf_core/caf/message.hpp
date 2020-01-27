/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <sstream>
#include <tuple>
#include <type_traits>

#include "caf/config.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/index_mapping.hpp"
#include "caf/make_counted.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/skip.hpp"
#include "caf/type_nr.hpp"

namespace caf {
class message_handler;

/// Describes a fixed-length, copy-on-write, type-erased
/// tuple with elements of any type.
class CAF_CORE_EXPORT message : public type_erased_tuple {
public:
  // -- member types -----------------------------------------------------------

  /// Raw pointer to content.
  using raw_ptr = detail::message_data*;

  /// Copy-on-write pointer to content.
  using data_ptr = detail::message_data::cow_ptr;

  // -- constructors, destructors, and assignment operators --------------------

  message() noexcept = default;
  message(none_t) noexcept;
  message(const message&) noexcept = default;
  message& operator=(const message&) noexcept = default;

  message(message&&) noexcept;
  message& operator=(message&&) noexcept;
  explicit message(data_ptr ptr) noexcept;

  ~message() override;

  // -- implementation of type_erased_tuple ------------------------------------

  void* get_mutable(size_t p) override;

  error load(size_t pos, deserializer& source) override;

  error_code<sec> load(size_t pos, binary_deserializer& source) override;

  size_t size() const noexcept override;

  rtti_pair type(size_t pos) const noexcept override;

  const void* get(size_t pos) const noexcept override;

  std::string stringify(size_t pos) const override;

  type_erased_value_ptr copy(size_t pos) const override;

  error save(size_t pos, serializer& sink) const override;

  error_code<sec> save(size_t pos, binary_serializer& sink) const override;

  bool shared() const noexcept override;

  error load(deserializer& source) override;

  error_code<sec> load(binary_deserializer& source) override;

  error save(serializer& sink) const override;

  error_code<sec> save(binary_serializer& sink) const override;

  // -- factories --------------------------------------------------------------

  /// Creates a new message by copying all elements in a type-erased tuple.
  static message copy(const type_erased_tuple& xs);

  // -- modifiers --------------------------------------------------------------

  /// Returns `handler(*this)`.
  optional<message> apply(message_handler handler);

  /// Forces the message to copy its content if there are more than
  /// one references to the content.
  inline void force_unshare() {
    vals_.unshare();
  }

  /// Returns a mutable reference to the content. Callers are responsible
  /// for unsharing content if necessary.
  inline data_ptr& vals() {
    return vals_;
  }

  /// Exchanges content of `this` and `other`.
  void swap(message& other) noexcept;

  /// Assigns new content.
  void reset(raw_ptr new_ptr = nullptr, bool add_ref = true) noexcept;

  // -- inline observers -------------------------------------------------------

  /// Returns a const pointer to the element at position `p`.
  inline const void* at(size_t p) const noexcept {
    CAF_ASSERT(vals_ != nullptr);
    return vals_->get(p);
  }

  /// Returns a reference to the content.
  inline const data_ptr& vals() const noexcept {
    return vals_;
  }

  /// Returns a reference to the content.
  inline const data_ptr& cvals() const noexcept {
    return vals_;
  }

  /// @cond PRIVATE

  /// @pre `!empty()`
  inline type_erased_tuple& content() {
    CAF_ASSERT(vals_ != nullptr);
    return vals_.unshared();
  }

  inline const type_erased_tuple& content() const {
    CAF_ASSERT(vals_ != nullptr);
    return *vals_;
  }

  /// Serializes the content of `x` as if `x` was an instance of `message`. The
  /// resulting output of `sink` can then be used to deserialize a `message`
  /// even if the serialized object had a different type.
  static error save(serializer& sink, const type_erased_tuple& x);

  static error_code<sec>
  save(binary_serializer& sink, const type_erased_tuple& x);

  /// @endcond

private:
  // -- private helpers --------------------------------------------------------

  template <size_t P>
  static bool match_elements_impl(std::integral_constant<size_t, P>,
                                  detail::type_list<>) noexcept {
    return true; // end of recursion
  }

  template <size_t P, class T, class... Ts>
  bool match_elements_impl(std::integral_constant<size_t, P>,
                           detail::type_list<T, Ts...>) const noexcept {
    std::integral_constant<size_t, P + 1> next_p;
    detail::type_list<Ts...> next_list;
    return match_element<T>(P) && match_elements_impl(next_p, next_list);
  }

  // -- member functions -------------------------------------------------------

  data_ptr vals_;
};

// -- related non-members ------------------------------------------------------

/// @relates message
CAF_CORE_EXPORT error inspect(serializer& sink, message& msg);

/// @relates message
CAF_CORE_EXPORT error_code<sec> inspect(binary_serializer& sink, message& msg);

/// @relates message
CAF_CORE_EXPORT error inspect(deserializer& source, message& msg);

/// @relates message
CAF_CORE_EXPORT error_code<sec>
inspect(binary_deserializer& source, message& msg);

/// @relates message
CAF_CORE_EXPORT std::string to_string(const message& msg);

} // namespace caf
