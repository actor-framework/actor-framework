// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf::policy {

/// Tag type to enable type-name-based serialization of type ID lists in
/// binary_serializer and binary_deserializer.
struct use_type_names_t {};

/// Tag to enable type-name-based serialization of type ID lists.
inline constexpr use_type_names_t use_type_names = use_type_names_t{};

} // namespace caf::policy
