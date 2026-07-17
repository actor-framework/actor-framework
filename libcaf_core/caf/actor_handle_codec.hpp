// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf {

/// Runtime-pluggable codec for serializing and deserializing actor handles.
class CAF_CORE_EXPORT actor_handle_codec {
public:
  virtual ~actor_handle_codec() noexcept;

  virtual bool save(serializer& sink, const strong_actor_ptr& ptr) = 0;

  virtual bool load(deserializer& source, strong_actor_ptr& ptr) = 0;

  virtual bool save(serializer& sink, const weak_actor_ptr& ptr) = 0;

  virtual bool load(deserializer& source, weak_actor_ptr& ptr) = 0;
};

} // namespace caf
