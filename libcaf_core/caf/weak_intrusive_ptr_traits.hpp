// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

namespace caf {

/// A trait for configuring `weak_intrusive_ptr<T>`. Specializations must
/// provide the following member types:
/// - `control_block_type` the type of the control block
/// - `managed_type` the type of objects managed by the control block
/// - `element_type`
template <class T>
struct weak_intrusive_ptr_traits;

} // namespace caf
