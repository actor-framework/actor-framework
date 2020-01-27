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
#include <utility>

#include "caf/byte.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/meta/annotation.hpp"
#include "caf/meta/save_callback.hpp"
#include "caf/read_inspector.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// @ingroup TypeSystem
/// Technology-independent serialization interface.
class CAF_CORE_EXPORT serializer : public read_inspector<serializer> {
public:
  // -- member types -----------------------------------------------------------

  using result_type = error;

  // -- constructors, destructors, and assignment operators --------------------

  explicit serializer(actor_system& sys) noexcept;

  explicit serializer(execution_unit* ctx = nullptr) noexcept;

  virtual ~serializer();

  // -- properties -------------------------------------------------------------

  auto context() const noexcept {
    return context_;
  }

  // -- interface functions ----------------------------------------------------

  /// Begins processing of an object. Saves the type information
  /// to the underlying storage.
  virtual result_type begin_object(uint16_t typenr, string_view type_name) = 0;

  /// Ends processing of an object.
  virtual result_type end_object() = 0;

  /// Begins processing of a sequence. Saves the size
  /// to the underlying storage when in saving mode, otherwise
  /// sets `num` accordingly.
  virtual result_type begin_sequence(size_t num) = 0;

  /// Ends processing of a sequence.
  virtual result_type end_sequence() = 0;

  /// Adds the primitive type `x` to the output.
  /// @param x The primitive value.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual result_type apply(bool x) = 0;

  /// @copydoc apply
  virtual result_type apply(int8_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(uint8_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(int16_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(uint16_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(int32_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(uint32_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(int64_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(uint64_t x) = 0;

  /// @copydoc apply
  virtual result_type apply(float x) = 0;

  /// @copydoc apply
  virtual result_type apply(double x) = 0;

  /// @copydoc apply
  virtual result_type apply(long double x) = 0;

  /// @copydoc apply
  virtual result_type apply(string_view x) = 0;

  /// @copydoc apply
  virtual result_type apply(const std::u16string& x) = 0;

  /// @copydoc apply
  virtual result_type apply(const std::u32string& x) = 0;

  template <class Enum, class = std::enable_if_t<std::is_enum<Enum>::value>>
  auto apply(Enum x) {
    return apply(static_cast<std::underlying_type_t<Enum>>(x));
  }

  /// Adds `x` as raw byte block to the output.
  /// @param x The byte sequence.
  /// @returns A non-zero error code on failure, `sec::success` otherwise.
  virtual result_type apply(span<const byte> x) = 0;

  /// Adds each boolean in `xs` to the output. Derived classes can override this
  /// member function to pack the booleans, for example to avoid using one byte
  /// for each value in a binary output format.
  virtual result_type apply(const std::vector<bool>& xs);

protected:
  /// Provides access to the ::proxy_registry and to the ::actor_system.
  execution_unit* context_;
};

} // namespace caf
