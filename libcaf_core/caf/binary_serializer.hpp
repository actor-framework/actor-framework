// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_handle_codec.hpp"
#include "caf/byte_writer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/inspector_config.hpp"
#include "caf/type_id.hpp"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace caf {

/// Base class for binary serializers.
class CAF_CORE_EXPORT binary_serializer_base : public byte_writer {
public:
  friend class binary_serializer;

  template <class>
  friend class binary_serializer_v2;

  binary_serializer_base(const binary_serializer_base&) = delete;

  binary_serializer_base& operator=(const binary_serializer_base&) = delete;

  ~binary_serializer_base() noexcept override;

  bool has_human_readable_format() const noexcept override;

  void set_error(error stop_reason) override;

  error& get_error() noexcept override;

  bool begin_object(type_id_t type, std::string_view name) noexcept override;

  bool end_object() override;

  bool begin_field(std::string_view name) noexcept override;

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

  bool value(const std::vector<bool>& x) override;

  bool value(type_id_list xs) override;

  bool update(size_t offset, const_byte_span content) override;

protected:
  bool save_names(type_id_list xs);

  bool save_ids(type_id_list xs);

private:
  binary_serializer_base() noexcept = default;

  virtual void do_append(const std::byte* data, size_t size) = 0;

  virtual byte_span mut_bytes() = 0;

  template <class T>
  bool int_value(T x);

  /// Error state for the serializer.
  error err_;
};

/// Serializes C++ objects into a sequence of bytes.
/// @note The binary data format may change between CAF versions and does not
///       perform any type checking at run-time. Thus the output of this
///       serializer is unsuitable for persistence layers.
class CAF_CORE_EXPORT binary_serializer final : public binary_serializer_base {
public:
  using super = binary_serializer_base;

  using container_type = byte_buffer;

  using value_type = std::byte;

  explicit binary_serializer(byte_buffer& buf) noexcept : buf_(buf) {
    // nop
  }

  binary_serializer(actor_system& sys, byte_buffer& buf) noexcept
    : buf_(buf), context_(&sys) {
    // nop
  }

  const_byte_span bytes() const override;

  void reset() override;

  size_t skip(size_t num_bytes) override;

  using super::value;

  bool value(const strong_actor_ptr& ptr) override;

private:
  void do_append(const std::byte* data, size_t size) override;

  byte_span mut_bytes() override;

  /// Stores the serialized output.
  byte_buffer& buf_;

  /// Optional context (the owning actor system) for the serializer.
  actor_system* context_ = nullptr;
};

/// Serializes C++ objects into a sequence of bytes.
/// @note The binary data format may change between CAF versions and does not
///       perform any type checking at run-time. Thus the output of this
///       serializer is unsuitable for persistence layers.
template <class Container>
class binary_serializer_v2 final : public binary_serializer_base {
public:
  using super = binary_serializer_base;

  // For `bytes`.
  static_assert(detail::contiguous_sequence_of<Container, std::byte>);

  // For `mut_bytes`.
  static_assert(detail::mutable_contiguous_sequence_of<Container, std::byte>);

  // For `skip` and `do_append`.
  static_assert(detail::back_insertable<Container, std::byte>);

  binary_serializer_v2(Container& buf, const inspector_config& config) noexcept
    : buf_(&buf),
      codec_(config.codec()),
      mapper_(&config.mapper()),
      use_type_names_(config.encoding() != object_type_encoding::type_id) {
    // nop
  }

  const_byte_span bytes() const override {
    return {buf_->data(), buf_->size()};
  }

  void reset() override {
    buf_->clear();
  }

  size_t skip(size_t num_bytes) override {
    auto offset = buf_->size();
    buf_->insert(buf_->end(), num_bytes, std::byte{0});
    return offset;
  }

  std::string_view to_type_name(type_id_t id) const override {
    return (*mapper_)(id);
  }

  using super::value;

  bool value(type_id_list xs) override {
    if (use_type_names_) {
      return this->save_names(xs);
    }
    return this->save_ids(xs);
  }

  bool value(const strong_actor_ptr& ptr) override {
    if (codec_) {
      return codec_->save(*this, ptr);
    }
    this->emplace_error(sec::no_actor_handle_codec);
    return false;
  }

private:
  void do_append(const std::byte* data, size_t size) override {
    buf_->insert(buf_->end(), data, data + size);
  }

  byte_span mut_bytes() override {
    return {buf_->data(), buf_->size()};
  }

  /// Stores the serialized output.
  Container* buf_;

  /// Optional codec for actor handles.
  caf::actor_handle_codec* codec_;

  /// Type ID mapper for the serializer.
  const type_id_mapper* mapper_;

  /// Whether to use type names instead of type IDs.
  bool use_type_names_;
};

} // namespace caf
