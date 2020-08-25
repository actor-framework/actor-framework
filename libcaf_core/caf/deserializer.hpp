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
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "caf/byte.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/load_inspector.hpp"
#include "caf/span.hpp"
#include "caf/type_id.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent deserialization interface.
class CAF_CORE_EXPORT deserializer : public load_inspector {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit deserializer(actor_system& sys) noexcept;

  explicit deserializer(execution_unit* ctx = nullptr) noexcept;

  virtual ~deserializer();

  // -- properties -------------------------------------------------------------

  auto context() const noexcept {
    return context_;
  }

  bool has_human_readable_format() const noexcept {
    return has_human_readable_format_;
  }

  // -- interface functions ----------------------------------------------------

  /// Begins processing of an object.
  virtual bool begin_object(string_view type) = 0;

  /// Ends processing of an object.
  virtual bool end_object() = 0;

  virtual bool begin_field(string_view name) = 0;

  virtual bool begin_field(string_view, bool& is_present) = 0;

  virtual bool
  begin_field(string_view name, span<const type_id_t> types, size_t& index)
    = 0;

  virtual bool begin_field(string_view name, bool& is_present,
                           span<const type_id_t> types, size_t& index)
    = 0;

  virtual bool end_field() = 0;

  /// Begins processing of a fixed-size sequence.
  virtual bool begin_tuple(size_t size) = 0;

  /// Ends processing of a sequence.
  virtual bool end_tuple() = 0;

  /// Begins processing of a sequence.
  virtual bool begin_sequence(size_t& size) = 0;

  /// Ends processing of a sequence.
  virtual bool end_sequence() = 0;

  /// Reads primitive value from the input.
  /// @param x The primitive value.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual bool value(bool& x) = 0;

  /// @copydoc value
  virtual bool value(int8_t&) = 0;

  /// @copydoc value
  virtual bool value(uint8_t&) = 0;

  /// @copydoc value
  virtual bool value(int16_t&) = 0;

  /// @copydoc value
  virtual bool value(uint16_t&) = 0;

  /// @copydoc value
  virtual bool value(int32_t&) = 0;

  /// @copydoc value
  virtual bool value(uint32_t&) = 0;

  /// @copydoc value
  virtual bool value(int64_t&) = 0;

  /// @copydoc value
  virtual bool value(uint64_t&) = 0;

  /// @copydoc value
  virtual bool value(float&) = 0;

  /// @copydoc value
  virtual bool value(double&) = 0;

  /// @copydoc value
  virtual bool value(long double&) = 0;

  /// @copydoc value
  virtual bool value(std::string&) = 0;

  /// @copydoc value
  virtual bool value(std::u16string&) = 0;

  /// @copydoc value
  virtual bool value(std::u32string&) = 0;

  /// Reads a byte sequence from the input.
  /// @param x The byte sequence.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual bool value(span<byte> x) = 0;

  /// Adds each boolean in `xs` to the output. Derived classes can override this
  /// member function to pack the booleans, for example to avoid using one byte
  /// for each value in a binary output format.
  virtual bool value(std::vector<bool>& xs);

  // -- DSL entry point --------------------------------------------------------

  template <class T>
  constexpr auto object(T&) noexcept {
    return object_t<deserializer>{type_name_or_anonymous<T>(), this};
  }

protected:
  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;

  /// Configures whether client code should assume human-readable output.
  bool has_human_readable_format_ = false;
};

} // namespace caf
