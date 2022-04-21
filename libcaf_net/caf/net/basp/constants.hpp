// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <cstdint>

namespace caf::net::basp {

/// @addtogroup BASP

/// The current BASP version.
/// @note BASP is not backwards compatible.
constexpr uint64_t version = 1;

/// Size of a BASP header in serialized form.
constexpr size_t header_size = 13;

/// @}

} // namespace caf::net::basp
