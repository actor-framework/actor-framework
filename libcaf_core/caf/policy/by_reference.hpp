// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf::policy {

/// Enforces by-reference semantics for a type or function.
struct by_reference_t {};

inline constexpr by_reference_t by_reference = by_reference_t{};

} // namespace caf::policy
