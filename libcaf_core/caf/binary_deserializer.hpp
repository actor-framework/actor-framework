// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_handle_codec.hpp"
#include "caf/byte_reader.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>

namespace caf {

/// Deserializes C++ objects from sequence of bytes. Does not perform
/// run-time type checks.
class CAF_CORE_EXPORT binary_deserializer final
  : public load_inspector_base<binary_deserializer, byte_reader> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  using super = load_inspector_base<binary_deserializer, byte_reader>;

  explicit binary_deserializer(const_byte_span input,
                               caf::actor_handle_codec* codec
                               = nullptr) noexcept;

  binary_deserializer(const void* buf, size_t size,
                      caf::actor_handle_codec* codec = nullptr) noexcept;

  ~binary_deserializer() noexcept override;

  binary_deserializer(const binary_deserializer&) = delete;

  binary_deserializer& operator=(const binary_deserializer&) = delete;

  [[nodiscard]] bool has_human_readable_format() const noexcept {
    return false;
  }

  [[nodiscard]] bool load_bytes(const_byte_span bytes) {
    return impl_->load_bytes(bytes);
  }

private:
  static constexpr size_t impl_storage_size = 48;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
