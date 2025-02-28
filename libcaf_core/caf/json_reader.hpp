// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/byte_reader.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf {

/// Deserializes an inspectable object from a JSON-formatted string.
class CAF_CORE_EXPORT json_reader final : public byte_reader {
public:
  // -- constructors, destructors, and assignment operators --------------------

  json_reader();

  explicit json_reader(actor_system& sys);

  json_reader(const json_reader&) = delete;

  json_reader& operator=(const json_reader&) = delete;

  ~json_reader() override;

  // -- properties -------------------------------------------------------------

  /// Returns the suffix for generating type annotation fields for variant
  /// fields. For example, CAF inserts field called "@foo${field_type_suffix}"
  /// for a variant field called "foo".
  [[nodiscard]] std::string_view field_type_suffix() const noexcept;

  /// Configures whether the writer omits empty fields.
  void field_type_suffix(std::string_view suffix) noexcept;

  /// Returns the type ID mapper used by the writer.
  [[nodiscard]] const type_id_mapper* mapper() const noexcept;

  /// Changes the type ID mapper for the writer.
  void mapper(const type_id_mapper* ptr) noexcept;

  // -- modifiers --------------------------------------------------------------

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  /// Parses @p json_text into an internal representation. After loading the
  /// JSON input, the reader is ready for attempting to deserialize inspectable
  /// objects.
  /// @warning The internal data structure keeps pointers into @p json_text.
  ///          Hence, the buffer pointed to by the string view must remain valid
  ///          until either destroying this reader or calling `reset`.
  /// @note Implicitly calls `reset`.
  bool load(std::string_view json_text);

  bool load_bytes(const_byte_span bytes) override;

  /// Reads the input stream @p input and parses the content into an internal
  /// representation. After loading the JSON input, the reader is ready for
  /// attempting to deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_from(std::istream& input);

  /// Parses the content of the file under the given @p path. After loading the
  /// content of the JSON file, the reader is ready for attempting to
  /// deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_file(const char* path);

  /// @copydoc load_file
  bool load_file(const std::string& path);

  /// Reverts the state of the reader back to where it was after calling `load`.
  /// @post The reader is ready for attempting to deserialize another
  ///       inspectable object.
  void revert();

  /// Removes any loaded JSON data and reclaims memory resources.
  void reset();

  // -- overrides --------------------------------------------------------------

  caf::actor_system* sys() const noexcept override;

  bool has_human_readable_format() const noexcept override;

  bool fetch_next_object_type(type_id_t& type) override;

  bool fetch_next_object_name(std::string_view& type_name) override;

  bool begin_object(type_id_t type, std::string_view name) override;

  bool end_object() override;

  bool begin_field(std::string_view) override;

  bool begin_field(std::string_view name, bool& is_present) override;

  bool begin_field(std::string_view name, span<const type_id_t> types,
                   size_t& index) override;

  bool begin_field(std::string_view name, bool& is_present,
                   span<const type_id_t> types, size_t& index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t& size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t& size) override;

  bool end_associative_array() override;

  bool value(std::byte& x) override;

  bool value(bool& x) override;

  bool value(int8_t& x) override;

  bool value(uint8_t& x) override;

  bool value(int16_t& x) override;

  bool value(uint16_t& x) override;

  bool value(int32_t& x) override;

  bool value(uint32_t& x) override;

  bool value(int64_t& x) override;

  bool value(uint64_t& x) override;

  bool value(float& x) override;

  bool value(double& x) override;

  bool value(long double& x) override;

  bool value(std::string& x) override;

  bool value(std::u16string& x) override;

  bool value(std::u32string& x) override;

  bool value(byte_span x) override;

private:
  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_[224];
};

} // namespace caf
