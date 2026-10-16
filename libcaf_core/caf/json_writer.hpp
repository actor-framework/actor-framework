// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_config.hpp"
#include "caf/text_writer.hpp"
#include "caf/type_id.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace caf::internal {

class json_writer_stack_entry;
class json_writer_util;

} // namespace caf::internal

namespace caf {

/// Base class for JSON writers.
class CAF_CORE_EXPORT json_writer_base : public text_writer {
public:
  friend class json_writer;

  friend class json_writer_v2;

  friend class internal::json_writer_util;

  using super = text_writer;

  json_writer_base(const json_writer_base&) = delete;

  json_writer_base& operator=(const json_writer_base&) = delete;

  ~json_writer_base() noexcept override;

  //// Returns the type ID mapper used by the JSON writer.
  [[nodiscard]] const type_id_mapper* mapper() const noexcept {
    return mapper_;
  }

  /// Sets the type ID mapper used by the JSON writer.
  /// @param ptr The new mapper to use. Passing `nullptr` restores the default
  ///            mapper.
  void mapper(const type_id_mapper* ptr) noexcept;

  [[nodiscard]] std::string_view str() const noexcept override;

  [[nodiscard]] size_t indentation() const noexcept override;

  void indentation(size_t factor) noexcept override;

  [[nodiscard]] bool compact() const noexcept override;

  [[nodiscard]] bool skip_empty_fields() const noexcept override;

  void skip_empty_fields(bool value) noexcept override;

  [[nodiscard]] bool skip_object_type_annotation() const noexcept override;

  void skip_object_type_annotation(bool value) noexcept override;

  [[nodiscard]] std::string_view field_type_suffix() const noexcept override;

  void field_type_suffix(std::string_view suffix) noexcept override;

  void reset() override;

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  bool has_human_readable_format() const noexcept override;

  std::string_view to_type_name(type_id_t id) const override;

  bool begin_object(type_id_t id, std::string_view name) override;

  bool end_object() override;

  bool begin_field(std::string_view name) override;

  bool begin_field(std::string_view name, bool is_present) override;

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t index) override;

  bool begin_field(std::string_view name, bool is_present,
                   std::span<const type_id_t> types, size_t index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t size) override;

  bool end_associative_array() override;

  using super::value;

  bool value(std::byte x) override;

  bool value(bool x) override;

  bool value(int8_t x) override;

  bool value(uint8_t x) override;

  bool value(int16_t x) override;

  bool value(uint16_t x) override;

  bool value(int32_t x) override;

  bool value(uint32_t x) override;

  bool value(int64_t x) override;

  bool value(uint64_t x) override;

  bool value(float x) override;

  bool value(double x) override;

  bool value(long double x) override;

  bool value(std::string_view x) override;

  bool value(const std::u16string& x) override;

  bool value(const std::u32string& x) override;

  bool value(const_byte_span x) override;

  bool value(type_id_list xs) override;

protected:
  json_writer_base();

  json_writer_base(const type_id_mapper& mapper);

private:
  using util = internal::json_writer_util;

  /// Configuration for the JSON writer.
  struct flags {
    bool keep_empty_fields : 1;
    bool skip_object_type_annotation : 1;
  };

  /// The current level of indentation in the JSON output.
  size_t indentation_level_ = 0;

  /// The number of spaces to use for each indentation level.
  size_t indentation_factor_ = 0;

  /// Buffers the generated JSON text output.
  std::vector<char> buf_;

  /// Maintains the state of the JSON writer while travesing the JSON structure.
  std::vector<internal::json_writer_stack_entry> stack_;

  /// Configuration flags for the JSON writer.
  flags flags_;

  /// The suffix used for field type annotations in the JSON output.
  std::string_view field_type_suffix_ = "-type";

  /// The type ID mapper used for serializing type annotations.
  const type_id_mapper* mapper_ = &get_default_type_id_mapper();

  /// Stores the last error encountered during JSON writing.
  error err_;
};

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer final : public json_writer_base {
public:
  using super = json_writer_base;

  json_writer() = default;

  explicit json_writer(actor_system& sys) : context_(&sys) {
    // nop
  }

  using super::value;

  bool value(const strong_actor_ptr& ptr) override;

private:
  actor_system* context_ = nullptr;
};

/// Serializes an inspectable object to a JSON-formatted string.
class CAF_CORE_EXPORT json_writer_v2 final : public json_writer_base {
public:
  using super = json_writer_base;

  json_writer_v2() = default;

  explicit json_writer_v2(const inspector_config& config)
    : super(config.mapper()), codec_(config.codec()) {
    // nop
  }

  using super::value;

  bool value(const strong_actor_ptr& ptr) override;

private:
  actor_handle_codec* codec_ = nullptr;
};

} // namespace caf
