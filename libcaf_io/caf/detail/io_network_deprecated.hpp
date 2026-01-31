// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

// Use __attribute__((deprecated)) instead of [[deprecated]] when combined with
// CAF_IO_EXPORT so that GCC accepts both visibility and deprecation on the
// same declaration (mixing [[attr]] and __attribute__ can fail in some orderings).

#ifdef CAF_SUPPRESS_DEPRECATION_WARNINGS
#  define CAF_IO_NETWORK_DEPRECATED
#else
#  define CAF_IO_NETWORK_DEPRECATED __attribute__((deprecated("use caf.net instead")))
#endif
