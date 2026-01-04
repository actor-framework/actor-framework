// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// Tag type to indicate that an intrusive smart pointer should add a new
/// reference, i.e., increase the reference count of the managed object by one.
struct add_ref_t {};

/// Tag to indicate that an intrusive smart pointer should add a new reference,
/// i.e., increase the reference count of the managed object by one.
inline constexpr add_ref_t add_ref = {};

} // namespace caf
