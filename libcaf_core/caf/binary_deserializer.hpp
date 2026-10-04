// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_handle_codec.hpp"
#include "caf/byte_reader.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_config.hpp"
#include "caf/type_id.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace caf {

/// Base class for binary deserializers.
class CAF_CORE_EXPORT binary_deserializer_base : public byte_reader {
public:
  friend class binary_deserializer;
  friend class binary_deserializer_v2;

  binary_deserializer_base(const binary_deserializer_base&) = delete;

  binary_deserializer_base& operator=(const binary_deserializer_base&) = delete;

  bool load_bytes(const_byte_span bytes) noexcept override;

  bool has_human_readable_format() const noexcept override;

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  bool fetch_next_object_type(type_id_t& type) noexcept override;

  bool begin_object(type_id_t type,
                    std::string_view pretty_class_name) noexcept override;

  bool end_object() noexcept override;

  bool begin_field(std::string_view name) noexcept override;

  bool begin_field(std::string_view name, bool& is_present) noexcept override;

  bool begin_field(std::string_view name, std::span<const type_id_t> types,
                   size_t& index) noexcept override;

  bool begin_field(std::string_view name, bool& is_present,
                   std::span<const type_id_t> types,
                   size_t& index) noexcept override;

  bool end_field() override;

  bool begin_tuple(size_t size) noexcept override;

  bool end_tuple() noexcept override;

  bool begin_key_value_pair() noexcept override;

  bool end_key_value_pair() noexcept override;

  bool begin_sequence(size_t& size) noexcept override;

  bool end_sequence() noexcept override;

  bool begin_associative_array(size_t& size) noexcept override;

  bool end_associative_array() noexcept override;

  bool value(std::byte& x) noexcept override;

  bool value(bool& x) noexcept override;

  bool value(int8_t& x) noexcept override;

  bool value(uint8_t& x) noexcept override;

  bool value(int16_t& x) noexcept override;

  bool value(uint16_t& x) noexcept override;

  bool value(int32_t& x) noexcept override;

  bool value(uint32_t& x) noexcept override;

  bool value(int64_t& x) noexcept override;

  bool value(uint64_t& x) noexcept override;

  bool value(float& x) noexcept override;

  bool value(double& x) noexcept override;

  bool value(long double& x) override;

  bool value(byte_span x) noexcept override;

  bool value(std::string& x) override;

  bool value(std::u16string& x) override;

  bool value(std::u32string& x) override;

  bool value(std::vector<bool>& x) override;

  bool value(type_id_list& xs) override;

private:
  binary_deserializer_base() noexcept = default;

  explicit binary_deserializer_base(const void* buf, size_t size) noexcept
    : pos_(static_cast<const std::byte*>(buf)), end_(pos_ + size) {
    // nop
  }

  size_t remaining() const noexcept;

  void skip(size_t num_bytes);

  bool range_check(size_t read_size) const noexcept;

  template <class T>
  bool int_value(T& x);

  template <class T>
  bool float_value(T& x);

  template <class T>
  void unsafe_int_value(T& x);

  /// Points to the current read position.
  const std::byte* pos_ = nullptr;

  /// Points to the end of the assigned memory block.
  const std::byte* end_ = nullptr;

  /// Error state for the deserializer.
  error err_;
};

/// Deserializes C++ objects from a sequence of bytes. Does not perform
/// run-time type checks.
class CAF_CORE_EXPORT binary_deserializer final
  : public binary_deserializer_base {
public:
  using super = binary_deserializer_base;

  binary_deserializer() noexcept = default;

  template <detail::char_or_byte_payload Container>
  explicit binary_deserializer(const Container& input) noexcept
    : binary_deserializer(input.data(), input.size()) {
    // nop
  }

  template <detail::char_or_byte_payload Container>
  binary_deserializer(actor_system& sys, const Container& input) noexcept
    : super(input.data(), input.size()), context_(&sys) {
    // nop
  }

  binary_deserializer(const void* buf, size_t size) noexcept
    : super(buf, size) {
    // nop
  }

  binary_deserializer(actor_system& sys, const void* buf, size_t size) noexcept
    : super(buf, size), context_(&sys) {
    // nop
  }

  type_id_t to_type_id(std::string_view name) const override;

  using super::value;

  bool value(strong_actor_ptr& ptr) override;

private:
  /// Optional context (the owning actor system) for the deserializer.
  actor_system* context_ = nullptr;
};

class CAF_CORE_EXPORT binary_deserializer_v2 final
  : public binary_deserializer_base {
public:
  using super = binary_deserializer_base;

  binary_deserializer_v2() noexcept = default;

  binary_deserializer_v2(const void* buf, size_t size,
                         const inspector_config& config) noexcept
    : super(buf, size),
      codec_(config.codec()),
      mapper_(&config.mapper()),
      use_type_names_(config.encoding() != object_type_encoding::type_id) {
    // nop
  }

  type_id_t to_type_id(std::string_view name) const override;

  using super::value;

  bool value(type_id_list& xs) override;

  bool value(strong_actor_ptr& ptr) override;

private:
  /// Optional codec for actor handles.
  caf::actor_handle_codec* codec_ = nullptr;

  /// Type ID mapper for the deserializer.
  const type_id_mapper* mapper_ = &get_default_type_id_mapper();

  /// Whether to use type names instead of type IDs.
  bool use_type_names_ = true;
};

} // namespace caf
