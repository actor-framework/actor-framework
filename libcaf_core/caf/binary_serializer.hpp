// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_handle_codec.hpp"
#include "caf/byte_writer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf {

/// Serializes C++ objects into a sequence of bytes.
/// @note The binary data format may change between CAF versions and does not
///       perform any type checking at run-time. Thus the output of this
///       serializer is unsuitable for persistence layers.
class CAF_CORE_EXPORT binary_serializer final
  : public save_inspector_base<binary_serializer, byte_writer> {
public:
  using super = save_inspector_base<binary_serializer, byte_writer>;

  explicit binary_serializer(byte_buffer& buf,
                             caf::actor_handle_codec* codec = nullptr) noexcept;

  ~binary_serializer() noexcept override;

  binary_serializer(const binary_serializer&) = delete;

  binary_serializer& operator=(const binary_serializer&) = delete;

  void reset() {
    impl_->reset();
  }

  [[nodiscard]] const_byte_span bytes() const noexcept {
    return impl_->bytes();
  }

  [[nodiscard]] bool has_human_readable_format() const noexcept {
    return false;
  }

  /// Jumps `num_bytes` forward by inserting `num_bytes` zeros at the end of the
  /// buffer.
  /// @returns the offset where the zero-bytes were inserted.
  [[nodiscard]] size_t skip(size_t num_bytes) {
    return impl_->skip(num_bytes);
  }

  /// Overrides the buffer at `offset` with `content`.
  /// @returns `true` if the buffer was large enough to hold `content`, `false`
  ///          otherwise.
  [[nodiscard]] bool update(size_t offset, const_byte_span content) noexcept {
    return impl_->update(offset, content);
  }

private:
  static constexpr size_t impl_storage_size = 40;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
