// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/json.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_config.hpp"
#include "caf/text_reader.hpp"
#include "caf/type_id.hpp"

#include <iosfwd>
#include <memory_resource>
#include <span>
#include <string>
#include <string_view>

namespace caf::internal {

class json_reader_util;
class json_reader_stack;

} // namespace caf::internal

namespace caf {

/// Base class for JSON readers.
class CAF_CORE_EXPORT json_reader_base : public text_reader {
public:
  friend class json_reader;

  friend class json_reader_v2;

  friend class internal::json_reader_util;

  // -- member types -----------------------------------------------------------

  using super = text_reader;

  json_reader_base(const json_reader_base&) = delete;

  json_reader_base& operator=(const json_reader_base&) = delete;

  ~json_reader_base() override;

  /// Returns the type ID mapper used by the writer.
  const type_id_mapper* mapper() const noexcept {
    return mapper_;
  }

  /// Changes the type ID mapper for the writer.
  void mapper(const type_id_mapper* ptr) noexcept;

  bool load(std::string_view json_text) override;

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

  /// Parses the content of the file under the given @p path. After loading the
  /// content of the JSON file, the reader is ready for attempting to
  /// deserialize inspectable objects.
  /// @note Implicitly calls `reset`.
  bool load_file(const std::string& path) {
    return load_file(path.c_str());
  }

  void revert() override;

  void reset() override;

  std::string_view field_type_suffix() const noexcept override;

  void field_type_suffix(std::string_view suffix) noexcept override;

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  bool has_human_readable_format() const noexcept override;

  type_id_t to_type_id(std::string_view name) const override;

  bool fetch_next_object_type(type_id_t& type) override;

  bool fetch_next_object_name(std::string_view& type_name) override;

  bool begin_object(type_id_t type, std::string_view name) override;

  bool end_object() override;

  bool begin_field(std::string_view name) override;

  bool begin_field(std::string_view name, bool& is_present) override;

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t& index) override;

  bool begin_field(std::string_view name, bool& is_present,
                   std::span<const type_id_t> types, size_t& index) override;

  bool end_field() override;

  bool begin_tuple(size_t size) override;

  bool end_tuple() override;

  bool begin_key_value_pair() override;

  bool end_key_value_pair() override;

  bool begin_sequence(size_t& size) override;

  bool end_sequence() override;

  bool begin_associative_array(size_t& size) override;

  bool end_associative_array() override;

  using super::value;

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

protected:
  json_reader_base();

  explicit json_reader_base(const type_id_mapper& mapper);

private:
  using util = internal::json_reader_util;

  std::pmr::monotonic_buffer_resource buf_;

  internal::json_reader_stack* st_ = nullptr;

  detail::json::value* root_ = nullptr;

  std::string_view field_type_suffix_ = "-type";

  const type_id_mapper* mapper_ = &get_default_type_id_mapper();

  error err_;
};

/// Deserializes an inspectable object from a JSON-formatted string.
class CAF_CORE_EXPORT json_reader final : public json_reader_base {
public:
  using super = json_reader_base;

  json_reader() = default;

  explicit json_reader(actor_system& sys) : context_(&sys) {
    // nop
  }

  using super::value;

  bool value(strong_actor_ptr& ptr) override;

private:
  actor_system* context_ = nullptr;
};

/// Deserializes an inspectable object from a JSON-formatted string.
class CAF_CORE_EXPORT json_reader_v2 final : public json_reader_base {
public:
  using super = json_reader_base;

  json_reader_v2() = default;

  explicit json_reader_v2(const inspector_config& config)
    : json_reader_base(config.mapper()), codec_(config.codec()) {
    // nop
  }

  using super::value;

  bool value(strong_actor_ptr& ptr) override;

private:
  actor_handle_codec* codec_ = nullptr;
};

} // namespace caf
