// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/caf_deprecated.hpp"
#include "caf/config.hpp"

/// Marks a declaration in `caf::io::network` as deprecated.
#define CAF_IO_NETWORK_DEPRECATED CAF_DEPRECATED("use caf.net instead")

/// Same as CAF_IO_NETWORK_DEPRECATED, but for type declarations that also use
/// CAF_IO_EXPORT. GCC rejects `[[deprecated]]` next to the visibility
/// attribute in either order, so this variant uses
/// `__attribute__((deprecated))` to keep both on the same declaration. Types
/// without CAF_IO_EXPORT (templates, nested types, forward declarations and
/// enums) should use
/// CAF_IO_NETWORK_DEPRECATED instead.
#ifdef CAF_SUPPRESS_DEPRECATION_WARNINGS
#  define CAF_IO_NETWORK_DEPRECATED_CLASS
#elif defined(CAF_MSVC)
#  define CAF_IO_NETWORK_DEPRECATED_CLASS                                      \
    __declspec(deprecated("use caf.net instead"))
#else
#  define CAF_IO_NETWORK_DEPRECATED_CLASS                                      \
    __attribute__((deprecated("use caf.net instead")))
#endif
