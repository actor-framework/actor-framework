// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

/// Evaluates to nothing.
#define CAF_PP_EMPTY()

/// Concatenates x and y into a single token.
#define CAF_PP_CAT(x, y) x##y

/// Evaluate x and y before concatenating into a single token.
#define CAF_PP_PASTE(x, y) CAF_PP_CAT(x, y)

/// Adds the current line number to make `NAME` unique.
#define CAF_PP_UNIFYN(name) CAF_PP_PASTE(name, __LINE__)

/// Evaluates to __COUNTER__. Allows delaying evaluation of __COUNTER__ in some
/// edge cases where it otherwise could increment the internal counter twice.
#define CAF_PP_COUNTER() __COUNTER__

#ifdef CAF_MSVC

/// Computes the number of arguments of a variadic pack.
#  define CAF_PP_SIZE(...)                                                     \
    CAF_PP_PASTE(CAF_PP_SIZE_I(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57,    \
                               56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, \
                               44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, \
                               32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, \
                               20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9,  \
                               8, 7, 6, 5, 4, 3, 2, 1, ),                      \
                 CAF_PP_EMPTY())

#else // CAF_MSVC

/// Computes the number of arguments of a variadic pack.
#  define CAF_PP_SIZE(...)                                                     \
    CAF_PP_SIZE_I(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, \
                  52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38,  \
                  37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23,  \
                  22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, \
                  6, 5, 4, 3, 2, 1, )

#endif // CAF_MSVC

#define CAF_PP_SIZE_I(e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12,   \
                      e13, e14, e15, e16, e17, e18, e19, e20, e21, e22, e23,   \
                      e24, e25, e26, e27, e28, e29, e30, e31, e32, e33, e34,   \
                      e35, e36, e37, e38, e39, e40, e41, e42, e43, e44, e45,   \
                      e46, e47, e48, e49, e50, e51, e52, e53, e54, e55, e56,   \
                      e57, e58, e59, e60, e61, e62, e63, size, ...)            \
  size

/// Allows overload-like macro functions.
#define CAF_PP_OVERLOAD(PREFIX, ...)                                           \
  CAF_PP_PASTE(PREFIX, CAF_PP_SIZE(__VA_ARGS__))

#define CAF_PP_EXPAND(...) __VA_ARGS__

// clang-format off
#define CAF_PP_STR_1(x1) #x1
#define CAF_PP_STR_2(x1, x2) #x1 ", " #x2
#define CAF_PP_STR_3(x1, x2, x3) #x1 ", " #x2 ", " #x3
#define CAF_PP_STR_4(x1, x2, x3, x4) #x1 ", " #x2 ", " #x3 ", " #x4
#define CAF_PP_STR_5(x1, x2, x3, x4, x5) #x1 ", " #x2 ", " #x3 ", " #x4 ", " #x5
#define CAF_PP_STR_6(x1, x2, x3, x4, x5, x6) #x1 ", " #x2 ", " #x3 ", " #x4 ", " #x5 ", " #x6
#define CAF_PP_STR_7(x1, x2, x3, x4, x5, x6, x7) #x1 ", " #x2 ", " #x3 ", " #x4 ", " #x5 ", " #x6 ", " #x7
#define CAF_PP_STR_8(x1, x2, x3, x4, x5, x6, x7, x8) #x1 ", " #x2 ", " #x3 ", " #x4 ", " #x5 ", " #x6 ", " #x7 ", " #x8
#define CAF_PP_STR_9(x1, x2, x3, x4, x5, x6, x7, x8, x9) #x1 ", " #x2 ", " #x3 ", " #x4 ", " #x5 ", " #x6 ", " #x7 ", " #x8 ", " #x9
// clang-format on

#define CAF_PP_XSTR(arg) CAF_PP_STR_1(arg)

#ifdef CAF_MSVC
#  define CAF_PP_STR(...)                                                      \
    CAF_PP_CAT(CAF_PP_OVERLOAD(CAF_PP_STR_, __VA_ARGS__)(__VA_ARGS__),         \
               CAF_PP_EMPTY())
#else
#  define CAF_PP_STR(...) CAF_PP_OVERLOAD(CAF_PP_STR_, __VA_ARGS__)(__VA_ARGS__)
#endif
