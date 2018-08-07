/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/term.hpp"

#include <iostream>

#include "caf/config.hpp"

#ifdef CAF_WINDOWS
#include <io.h>
#include <windows.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace caf {

namespace {

#ifdef CAF_WINDOWS

// windows terminals do no support bold fonts

constexpr int win_black = 0;
constexpr int win_red = FOREGROUND_INTENSITY | FOREGROUND_RED;
constexpr int win_green = FOREGROUND_INTENSITY | FOREGROUND_GREEN;
constexpr int win_blue = FOREGROUND_INTENSITY | FOREGROUND_BLUE;
constexpr int win_yellow = win_red | win_green;
constexpr int win_magenta = win_red | win_blue;
constexpr int win_cyan = win_green | win_blue;
constexpr int win_white = win_red | win_cyan;

int tty_codes[] = {
  -1,          // reset
  -2,          // reset_endl
  win_black,   // black
  win_red,     // red
  win_green,   // green
  win_yellow,  // yellow
  win_blue,    // blue
  win_magenta, // magenta
  win_cyan,    // cyan
  win_white,   // white
  win_black,   // bold black
  win_red,     // bold red
  win_green,   // bold green
  win_yellow,  // bold yellow
  win_blue,    // bold blue
  win_magenta, // bold magenta
  win_cyan,    // bold cyan
  win_white    // bold white
};

void set_term_color(std::ostream& out, int c) {
  static WORD default_color = 0xFFFF;
  auto hdl = GetStdHandle(&out == &std::cout ? STD_OUTPUT_HANDLE
                                             : STD_ERROR_HANDLE);
  if (default_color == 0xFFFF) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(hdl, &info))
      return;
    default_color = info.wAttributes;
  }
  // always pick background from the default color
  auto x = c < 0 ? default_color
                 : static_cast<WORD>((0xF0 & default_color) | (0x0F & c));
  SetConsoleTextAttribute(hdl, x);
  if (c == -2)
    out << '\n';
}

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define ISATTY_FUN ::_isatty

#else // POSIX-compatible terminals

const char* tty_codes[] = {
  "\033[0m",         // reset
  "\033[0m\n",       // reset_endl
  "\033[30m",        // black
  "\033[31m",        // red
  "\033[32m",        // green
  "\033[33m",        // yellow
  "\033[34m",        // blue
  "\033[35m",        // magenta
  "\033[36m",        // cyan
  "\033[37m",        // white
  "\033[1m\033[30m", // bold_black
  "\033[1m\033[31m", // bold_red
  "\033[1m\033[32m", // bold_green
  "\033[1m\033[33m", // bold_yellow
  "\033[1m\033[34m", // bold_blue
  "\033[1m\033[35m", // bold_magenta
  "\033[1m\033[36m", // bold_cyan
  "\033[1m\033[37m"  // bold_white
};

void set_term_color(std::ostream& out, const char* x) {
  out << x;
}

#define ISATTY_FUN ::isatty

#endif

bool is_tty(const std::ostream& out) {
  if (&out == &std::cout)
    return ISATTY_FUN(STDOUT_FILENO) != 0;
  if (&out == &std::cerr || &out == &std::clog)
    return ISATTY_FUN(STDERR_FILENO) != 0;
  return false;
}

} // namespace <anonymous>

std::ostream& operator<<(std::ostream& out, term x) {
  if (is_tty(out))
    set_term_color(out, tty_codes[static_cast<size_t>(x)]);
  else if (x == term::reset_endl)
    out << '\n';
  return out;
}

} // namespace caf
