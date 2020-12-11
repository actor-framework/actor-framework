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

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "caf/byte.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/squashed_int.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent serialization interface.
class CAF_CORE_EXPORT serializer : public save_inspector_base<serializer> {
public:
  // -- member types -----------------------------------------------------------

  using super = save_inspector_base<serializer>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit serializer(actor_system& sys) noexcept;

  explicit serializer(execution_unit* ctx = nullptr) noexcept;

  virtual ~serializer();

  // -- properties -------------------------------------------------------------

  auto context() const noexcept {
    return context_;
  }

  bool has_human_readable_format() const noexcept {
    return has_human_readable_format_;
  }

  // -- interface functions ----------------------------------------------------

  /// Begins processing of an object. May save the type information to the
  /// underlying storage to allow a @ref deserializer to retrieve and check the
  /// type information for data formats that provide deserialization.
  virtual bool begin_object(type_id_t type, string_view name) = 0;

  /// Ends processing of an object.
  virtual bool end_object() = 0;

  virtual bool begin_field(string_view) = 0;

  virtual bool begin_field(string_view name, bool is_present) = 0;

  virtual bool
  begin_field(string_view name, span<const type_id_t> types, size_t index)
    = 0;

  virtual bool begin_field(string_view name, bool is_present,
                           span<const type_id_t> types, size_t index)
    = 0;

  virtual bool end_field() = 0;

  /// Begins processing of a tuple.
  virtual bool begin_tuple(size_t size) = 0;

  /// Ends processing of a tuple.
  virtual bool end_tuple() = 0;

  /// Begins processing of a tuple with two elements, whereas the first element
  /// represents the key in an associative array.
  /// @note the default implementation calls `begin_tuple(2)`.
  virtual bool begin_key_value_pair();

  /// Ends processing of a key-value pair after both values were written.
  /// @note the default implementation calls `end_tuple()`.
  virtual bool end_key_value_pair();

  /// Begins processing of a sequence. Saves the size to the underlying storage.
  virtual bool begin_sequence(size_t size) = 0;

  /// Ends processing of a sequence.
  virtual bool end_sequence() = 0;

  /// Begins processing of an associative array (map).
  /// @note the default implementation calls `begin_sequence(size)`.
  virtual bool begin_associative_array(size_t size);

  /// Ends processing of an associative array (map).
  /// @note the default implementation calls `end_sequence()`.
  virtual bool end_associative_array();

  /// Adds `x` to the output.
  /// @param x A value for a builtin type.
  /// @returns `true` on success, `false` otherwise.
  virtual bool value(byte x) = 0;

  /// @copydoc value
  virtual bool value(bool x) = 0;

  /// @copydoc value
  virtual bool value(int8_t x) = 0;

  /// @copydoc value
  virtual bool value(uint8_t x) = 0;

  /// @copydoc value
  virtual bool value(int16_t x) = 0;

  /// @copydoc value
  virtual bool value(uint16_t x) = 0;

  /// @copydoc value
  virtual bool value(int32_t x) = 0;

  /// @copydoc value
  virtual bool value(uint32_t x) = 0;

  /// @copydoc value
  virtual bool value(int64_t x) = 0;

  /// @copydoc value
  virtual bool value(uint64_t x) = 0;

  /// @copydoc value
  template <class T>
  std::enable_if_t<std::is_integral<T>::value, bool> value(T x) {
    return value(static_cast<detail::squashed_int_t<T>>(x));
  }

  /// @copydoc value
  virtual bool value(float x) = 0;

  /// @copydoc value
  virtual bool value(double x) = 0;

  /// @copydoc value
  virtual bool value(long double x) = 0;

  /// @copydoc value
  virtual bool value(string_view x) = 0;

  /// @copydoc value
  virtual bool value(const std::u16string& x) = 0;

  /// @copydoc value
  virtual bool value(const std::u32string& x) = 0;

  /// Adds `x` as raw byte block to the output.
  /// @param x The byte sequence.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual bool value(span<const byte> x) = 0;

  using super::list;

  /// Adds each boolean in `xs` to the output. Derived classes can override this
  /// member function to pack the booleans, for example to avoid using one byte
  /// for each value in a binary output format.
  virtual bool list(const std::vector<bool>& xs);

protected:
  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;

  /// Configures whether client code should assume human-readable output.
  bool has_human_readable_format_ = false;
};

} // namespace caf
