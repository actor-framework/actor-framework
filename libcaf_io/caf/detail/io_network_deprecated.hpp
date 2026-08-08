// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"
#include "caf/config.hpp"

/// Marks a declaration in `caf::io::network` as deprecated.
#define CAF_IO_NETWORK_DEPRECATED CAF_DEPRECATED("use caf.net instead")

/// Same as ::CAF_IO_NETWORK_DEPRECATED, but for type declarations, i.e.,
/// classes, structs and enums. Uses `__attribute__((deprecated))` instead of
/// `[[deprecated]]` so that GCC accepts both visibility and deprecation on the
/// same declaration (mixing `[[attr]]` and `__attribute__` can fail in some
/// orderings).
#ifdef CAF_SUPPRESS_DEPRECATION_WARNINGS
#  define CAF_IO_NETWORK_DEPRECATED_CLASS
#elif defined(CAF_MSVC)
#  define CAF_IO_NETWORK_DEPRECATED_CLASS                                      \
    __declspec(deprecated("use caf.net instead"))
#else
#  define CAF_IO_NETWORK_DEPRECATED_CLASS                                      \
    __attribute__((deprecated("use caf.net instead")))
#endif
