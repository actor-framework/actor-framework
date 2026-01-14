// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#ifdef CAF_SUPPRESS_DEPRECATION_WARNINGS

#  define CAF_DEPRECATED(msg)

#else

#  define CAF_DEPRECATED(msg) [[deprecated(msg)]]

#endif
