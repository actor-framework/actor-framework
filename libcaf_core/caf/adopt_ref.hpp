// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// Tag type to indicate that an intrusive smart pointer should adopt the
/// reference, i.e., store the pointer without increasing the reference count of
/// the managed object.
struct adopt_ref_t {};

/// Tag to indicate that an intrusive smart pointer should adopt the reference,
/// i.e., store the pointer without increasing the reference count of the
/// managed object.
inline constexpr adopt_ref_t adopt_ref = {};

} // namespace caf
