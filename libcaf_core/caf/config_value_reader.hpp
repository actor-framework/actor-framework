// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/deserializer.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/load_inspector_base.hpp"

#include <cstddef>

namespace caf {

/// Extracts objects from a @ref config_value.
class CAF_CORE_EXPORT config_value_reader final
  : public load_inspector_base<config_value_reader, deserializer> {
public:
  using super = load_inspector_base<config_value_reader, deserializer>;

  explicit config_value_reader(const config_value* input,
                               caf::actor_handle_codec* codec = nullptr);

  ~config_value_reader() noexcept override;

  config_value_reader(const config_value_reader&) = delete;

  config_value_reader& operator=(const config_value_reader&) = delete;

  bool has_human_readable_format() const noexcept {
    return true;
  }

private:
  static constexpr size_t impl_storage_size = 88;

  /// Storage for the implementation object.
  alignas(std::max_align_t) std::byte impl_storage_[impl_storage_size];
};

} // namespace caf
