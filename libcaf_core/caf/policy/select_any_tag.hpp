// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf::policy {

struct select_any_tag_t {};

constexpr auto select_any_tag = select_any_tag_t{};

} // namespace caf::policy
