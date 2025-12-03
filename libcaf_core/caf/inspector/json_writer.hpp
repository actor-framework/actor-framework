// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector/byte_writer.hpp"
#include "caf/placement_ptr.hpp"

#include <cstddef>

namespace caf::inspector {

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer : public byte_writer {
public:
  // -- constructors, destructors, and assignment operators --------------------

  ~json_writer() override;

  // -- properties -------------------------------------------------------------

  /// Returns a string view into the internal buffer.
  /// @warning This view becomes invalid when calling any non-const member
  ///          function on the writer object.
  [[nodiscard]] virtual std::string_view str() const noexcept = 0;

  /// Returns the current indentation factor.
  [[nodiscard]] virtual size_t indentation() const noexcept = 0;

  /// Sets the indentation level.
  /// @param factor The number of spaces to add to each level of indentation. A
  ///               value of 0 (the default) disables indentation, printing the
  ///               entire JSON output into a single line.
  virtual void indentation(size_t factor) noexcept = 0;

  /// Returns whether the writer generates compact JSON output without any
  /// spaces or newlines to separate values.
  [[nodiscard]] bool compact() const noexcept {
    return indentation() == 0;
  }

  /// Returns whether the writer omits empty fields entirely (true) or renders
  /// empty fields as `$field: null` (false).
  [[nodiscard]] virtual bool skip_empty_fields() const noexcept = 0;

  /// Configures whether the writer omits empty fields.
  virtual void skip_empty_fields(bool value) noexcept = 0;

  /// Returns whether the writer omits `@type` annotations for JSON objects.
  [[nodiscard]] virtual bool skip_object_type_annotation() const noexcept = 0;

  /// Configures whether the writer omits `@type` annotations for JSON objects.
  virtual void skip_object_type_annotation(bool value) noexcept = 0;

  /// Returns the suffix for generating type annotation fields for variant
  /// fields. For example, CAF inserts field called "@foo${field_type_suffix}"
  /// for a variant field called "foo".
  [[nodiscard]] virtual std::string_view field_type_suffix() const noexcept = 0;

  /// Configures whether the writer omits empty fields.
  virtual void field_type_suffix(std::string_view suffix) noexcept = 0;

  /// Returns the type ID mapper used by the writer.
  [[nodiscard]] virtual const type_id_mapper* mapper() const noexcept = 0;

  /// Changes the type ID mapper for the writer.
  virtual void mapper(const type_id_mapper* ptr) noexcept = 0;
};

} // namespace caf::inspector
