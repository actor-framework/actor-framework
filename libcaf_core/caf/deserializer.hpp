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
#include "caf/fwd.hpp"
#include "caf/span.hpp"
#include "caf/write_inspector.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent deserialization interface.
class deserializer : public write_inspector<deserializer> {
public:
  // -- member types -----------------------------------------------------------

  using result_type = error;

  // -- constructors, destructors, and assignment operators --------------------

  explicit deserializer(actor_system& sys) noexcept;

  explicit deserializer(execution_unit* ctx = nullptr) noexcept;

  virtual ~deserializer();

  // -- properties -------------------------------------------------------------

  auto context() const noexcept {
    return context_;
  }

  // -- interface functions ----------------------------------------------------

  /// Begins processing of an object.
  virtual result_type begin_object(uint16_t& typenr, std::string& type_name)
    = 0;

  /// Ends processing of an object.
  virtual result_type end_object() = 0;

  /// Begins processing of a sequence.
  virtual result_type begin_sequence(size_t& size) = 0;

  /// Ends processing of a sequence.
  virtual result_type end_sequence() = 0;

  /// Reads primitive value from the input.
  /// @param x The primitive value.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual result_type apply(bool& x) = 0;

  /// @copydoc apply
  virtual result_type apply(int8_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(uint8_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(int16_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(uint16_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(int32_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(uint32_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(int64_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(uint64_t&) = 0;

  /// @copydoc apply
  virtual result_type apply(float&) = 0;

  /// @copydoc apply
  virtual result_type apply(double&) = 0;

  /// @copydoc apply
  virtual result_type apply(long double&) = 0;

  /// @copydoc apply
  virtual result_type apply(std::string&) = 0;

  /// @copydoc apply
  virtual result_type apply(std::u16string&) = 0;

  /// @copydoc apply
  virtual result_type apply(std::u32string&) = 0;

  /// @copydoc apply
  template <class Enum, class = std::enable_if_t<std::is_enum<Enum>::value>>
  auto apply(Enum& x) {
    return apply(reinterpret_cast<std::underlying_type_t<Enum>&>(x));
  }

  /// Reads a byte sequence from the input.
  /// @param x The byte sequence.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual result_type apply(span<byte> x) = 0;

  /// Adds each boolean in `xs` to the output. Derived classes can override this
  /// member function to pack the booleans, for example to avoid using one byte
  /// for each value in a binary output format.
  virtual result_type apply(std::vector<bool>& xs) noexcept;

protected:
  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;
};

} // namespace caf
