// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_handle_codec.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

/// Default implementation for serializing and deserializing actor handles.
class CAF_CORE_EXPORT default_actor_handle_codec final
  : public actor_handle_codec {
public:
  explicit default_actor_handle_codec(actor_system& sys) noexcept;

  bool save(serializer& sink, const strong_actor_ptr& ptr) override;

  bool load(deserializer& source, strong_actor_ptr& ptr) override;

  bool save(serializer& sink, const weak_actor_ptr& ptr) override;

  bool load(deserializer& source, weak_actor_ptr& ptr) override;

private:
  actor_system* sys_;
};

} // namespace caf::detail
