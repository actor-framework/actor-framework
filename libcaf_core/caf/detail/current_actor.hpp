// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

CAF_CORE_EXPORT abstract_actor* current_actor() noexcept;

CAF_CORE_EXPORT void current_actor(abstract_actor* ptr) noexcept;

class CAF_CORE_EXPORT current_actor_guard {
public:
  [[nodiscard]] explicit current_actor_guard(abstract_actor* ptr) noexcept;

  current_actor_guard(const current_actor_guard&) = delete;

  current_actor_guard& operator=(const current_actor_guard&) = delete;

  ~current_actor_guard() noexcept;

private:
  abstract_actor* prev_;
};

} // namespace caf::detail
