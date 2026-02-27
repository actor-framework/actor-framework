// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <string>

namespace caf::detail {

CAF_CORE_EXPORT std::string pretty_type_name(const char* class_name);

} // namespace caf::detail
