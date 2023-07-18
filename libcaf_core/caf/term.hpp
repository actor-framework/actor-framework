// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <iosfwd>

namespace caf {

/// Terminal color and font face options.
enum class term {
  /// Resets the color to the default color and the font weight to normal.
  reset,
  /// Like `reset` but also prints a newline.
  reset_endl,
  /// Sets the terminal color to black.
  black,
  /// Sets the terminal color to red.
  red,
  /// Sets the terminal color to green.
  green,
  /// Sets the terminal color to yellow.
  yellow,
  /// Sets the terminal color to blue.
  blue,
  /// Sets the terminal color to magenta.
  magenta,
  /// Sets the terminal color to cyan.
  cyan,
  /// Sets the terminal color to white.
  white,
  /// Sets the terminal color to black and the font weight to bold.
  bold_black,
  /// Sets the terminal color to red and the font weight to bold.
  bold_red,
  /// Sets the terminal color to green and the font weight to bold.
  bold_green,
  /// Sets the terminal color to yellow and the font weight to bold.
  bold_yellow,
  /// Sets the terminal color to blue and the font weight to bold.
  bold_blue,
  /// Sets the terminal color to magenta and the font weight to bold.
  bold_magenta,
  /// Sets the terminal color to cyan and the font weight to bold.
  bold_cyan,
  /// Sets the terminal color to white and the font weight to bold.
  bold_white
};

CAF_CORE_EXPORT std::ostream& operator<<(std::ostream& out, term x);

} // namespace caf
