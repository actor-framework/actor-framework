/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2020 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

// Based on work of others. Retrieved on 2020-07-01 from
// https://github.com/Genivia/ugrep/blob/d2fb133/src/glob.cpp.
// Original header / license:

/******************************************************************************\
* Copyright (c) 2019, Robert van Engelen, Genivia Inc. All rights reserved.    *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions are met:  *
*                                                                              *
*   (1) Redistributions of source code must retain the above copyright notice, *
*       this list of conditions and the following disclaimer.                  *
*                                                                              *
*   (2) Redistributions in binary form must reproduce the above copyright      *
*       notice, this list of conditions and the following disclaimer in the    *
*       documentation and/or other materials provided with the distribution.   *
*                                                                              *
*   (3) The name of the author may not be used to endorse or promote products  *
*       derived from this software without specific prior written permission.  *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF         *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO   *
* EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,       *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, *
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;  *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,     *
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR      *
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF       *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                   *
\******************************************************************************/

#include "caf/detail/glob_match.hpp"

#include <cstdio>
#include <cstring>

#include "caf/config.hpp"

namespace caf::detail {

namespace {

static constexpr char path_separator =
#ifdef CAF_WINDOWS
  '\\'
#else
  '/'
#endif
  ;

int utf8(const char** s);

// match text against glob, return true or false
bool match(const char* text, const char* glob) {
  // to iteratively backtrack on *
  const char* text1_backup = nullptr;
  const char* glob1_backup = nullptr;
  // to iteratively backtrack on **
  const char* text2_backup = nullptr;
  const char* glob2_backup = nullptr;
  // match until end of text
  while (*text != '\0') {
    switch (*glob) {
      case '*':
        if (*++glob == '*') {
          // trailing ** match everything after /
          if (*++glob == '\0')
            return true;

          // ** followed by a / match zero or more directories
          if (*glob != '/')
            return false;

          // iteratively backtrack on **
          text1_backup = NULL;
          glob1_backup = NULL;
          text2_backup = text;
          glob2_backup = ++glob;
          continue;
        }
        // iteratively backtrack on *
        text1_backup = text;
        glob1_backup = glob;
        continue;
      case '?':
        // match anything except /
        if (*text == path_separator)
          break;

        utf8(&text);
        glob++;
        continue;
      case '[': {
        int chr = utf8(&text), last = 0x10ffff;
        // match anything except /
        if (chr == path_separator)
          break;
        bool matched = false;
        bool reverse = glob[1] == '^' || glob[1] == '!';
        // inverted character class
        if (reverse)
          glob++;
        glob++;
        // match character class
        while (*glob != '\0' && *glob != ']')
          if (last < 0x10ffff && *glob == '-' && glob[1] != ']'
                  && glob[1] != '\0'
                ? chr <= utf8(&++glob) && chr >= last
                : chr == (last = utf8(&glob)))
            matched = true;
        if (matched == reverse)
          break;
        if (*glob)
          glob++;
        continue;
      }
      case '\\':
        // literal match \-escaped character
        glob++;
        [[fallthrough]];
      default:
#ifdef CAF_WINDOWS
        if (*glob != *text && !(*glob == '/' && *text == '\\'))
#else
        if (*glob != *text)
#endif
          break;
        text++;
        glob++;
        continue;
    }
    if (glob1_backup != NULL && *text1_backup != path_separator) {
      // backtrack on the last *, do not jump over /
      text = ++text1_backup;
      glob = glob1_backup;
      continue;
    }
    if (glob2_backup != NULL) {
      // backtrack on the last **
      text = ++text2_backup;
      glob = glob2_backup;
      continue;
    }
    return false;
  }
  while (*glob == '*')
    glob++;
  return *glob == '\0';
}

int utf8(const char** s) {
  int c1, c2, c3, c = (unsigned char) **s;
  if (c != '\0')
    (*s)++;
  if (c < 0x80)
    return c;
  c1 = (unsigned char) **s;
  if (c < 0xC0 || (c == 0xC0 && c1 != 0x80) || c == 0xC1 || (c1 & 0xC0) != 0x80)
    return 0xFFFD;
  if (c1 != '\0')
    (*s)++;
  c1 &= 0x3F;
  if (c < 0xE0)
    return (((c & 0x1F) << 6) | c1);
  c2 = (unsigned char) **s;
  if ((c == 0xE0 && c1 < 0x20) || (c2 & 0xC0) != 0x80)
    return 0xFFFD;
  if (c2 != '\0')
    (*s)++;
  c2 &= 0x3F;
  if (c < 0xF0)
    return (((c & 0x0F) << 12) | (c1 << 6) | c2);
  c3 = (unsigned char) **s;
  if (c3 != '\0')
    (*s)++;
  if ((c == 0xF0 && c1 < 0x10) || (c == 0xF4 && c1 >= 0x10) || c >= 0xF5
      || (c3 & 0xC0) != 0x80)
    return 0xFFFD;
  return (((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | (c3 & 0x3F));
}

} // namespace

// pathname or basename matching, returns true or false
bool glob_match(const char* str, const char* glob) {
  // an empty glob matches nothing and an empty string matches no glob
  if (glob[0] == '\0' || str[0] == '\0')
    return false;
  // if str starts with ./ then remove these pairs
  while (str[0] == '.' && str[1] == path_separator)
    str += 2;
  // if str starts with / then remove it
  if (str[0] == path_separator)
    ++str;
  // a leading / in the glob means globbing the str after removing the /
  if (glob[0] == '/')
    ++glob;
  return match(str, glob);
}

} // namespace caf::detail
