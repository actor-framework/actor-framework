/******************************************************************************
 *                   _________________________                                *
 *                   __  ____/__    |__  ____/    C++                         *
 *                   _  /    __  /| |_  /_        Actor                       *
 *                   / /___  _  ___ |  __/        Framework                   *
 *                   ____/  /_/  |_/_/                                       *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

/******************************************************************************\
 * Based on work by the mingw-w64 project;                  *
 * original header:                               *
 *                                      *
 * Copyright (c) 2012 mingw-w64 project                     *
 *                                      *
 * Contributing author: Kai Tietz                       *
 *                                      *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the    *
 * Software is furnished to do so, subject to the following conditions:     *
 *                                      *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.            *
 *                                      *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL  *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING  *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER    *
 * DEALINGS IN THE SOFTWARE.                          *
\ ******************************************************************************/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>

#include <sstream>
#include <iostream>

#include "cppa/detail/execinfo_windows.hpp"

namespace cppa {
namespace detail {

int backtrace(void** buffer, int size) {
  if (size <= 0) return 0;
  auto frames = CaptureStackBackTrace (0, (DWORD) size, buffer, NULL);
  return static_cast<int>(frames);
}

void backtrace_symbols_fd(void* const* buffer, int size, int fd) {
  std::ostringstream out;
  for (int i = 0; i < size; i++) {
    out << "[" << std::hex << reinterpret_cast<size_t>(buffer[i])
      << "]" << std::endl;
    auto s = out.str();
    write(fd, s.c_str(), s.size());
  }
  _commit(fd);
}

} // namespace detail
} // namespace cppa

