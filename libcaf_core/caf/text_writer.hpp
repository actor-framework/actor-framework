// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/byte_writer.hpp"
#include "caf/detail/core_export.hpp"

#include <cstddef>
#include <string_view>

namespace caf {

class type_id_mapper;

/// Serializes inspectable objects to a text-based format.
class CAF_CORE_EXPORT text_writer : public byte_writer {
public:
  using byte_writer::byte_writer;

  ~text_writer() override;

  /// Returns a string view into the internal buffer.
  /// @warning This view becomes invalid when calling any non-const member
  ///          function on the writer object.
  [[nodiscard]] virtual std::string_view str() const noexcept = 0;

  /// Returns the current indentation factor.
  [[nodiscard]] virtual size_t indentation() const noexcept = 0;

  /// Sets the indentation level.
  /// @param factor The number of spaces to add to each level of indentation. A
  ///               value of 0 (the default) disables indentation, printing the
  ///               entire output into a single line.
  virtual void indentation(size_t factor) noexcept = 0;

  /// Returns whether the writer generates compact output without any spaces or
  /// newlines to separate values.
  [[nodiscard]] virtual bool compact() const noexcept = 0;

  /// Returns whether the writer omits empty fields entirely (true) or renders
  /// empty fields with a null value (false).
  [[nodiscard]] virtual bool skip_empty_fields() const noexcept = 0;

  /// Configures whether the writer omits empty fields.
  virtual void skip_empty_fields(bool value) noexcept = 0;

  /// Returns whether the writer omits type annotations for objects.
  [[nodiscard]] virtual bool skip_object_type_annotation() const noexcept = 0;

  /// Configures whether the writer omits type annotations for objects.
  virtual void skip_object_type_annotation(bool value) noexcept = 0;

  /// Returns the suffix for generating type annotation fields for variant
  /// fields.
  [[nodiscard]] virtual std::string_view field_type_suffix() const noexcept = 0;

  /// Configures the suffix for generating type annotation fields for variant
  /// fields.
  virtual void field_type_suffix(std::string_view suffix) noexcept = 0;

  /// Returns the type ID mapper used by the writer.
  [[nodiscard]] virtual const type_id_mapper* mapper() const noexcept = 0;

  /// Changes the type ID mapper for the writer.
  virtual void mapper(const type_id_mapper* ptr) noexcept = 0;
};

} // namespace caf
