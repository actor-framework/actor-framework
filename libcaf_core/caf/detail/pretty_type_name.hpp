// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <string>
#include <typeinfo>

namespace caf::detail {

CAF_CORE_EXPORT void prettify_type_name(std::string& class_name);

CAF_CORE_EXPORT void prettify_type_name(std::string& class_name,
                                        const char* input_class_name);

CAF_CORE_EXPORT std::string pretty_type_name(const std::type_info& x);

} // namespace caf::detail
