// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/save_inspector_base.hpp"
#include "caf/serializer.hpp"

#include <cstddef>

namespace caf {

/// Serializes an objects into a @ref config_value.
class CAF_CORE_EXPORT config_value_writer final
  : public save_inspector_base<config_value_writer, serializer> {
public:
  using super = save_inspector_base<config_value_writer, serializer>;

  explicit config_value_writer(config_value* dst,
                               caf::actor_handle_codec* codec = nullptr);

  ~config_value_writer() noexcept override;

  config_value_writer(const config_value_writer&) = delete;

  config_value_writer& operator=(const config_value_writer&) = delete;

  bool has_human_readable_format() const noexcept {
    return true;
  }

private:
  static constexpr size_t impl_storage_size = 64;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
