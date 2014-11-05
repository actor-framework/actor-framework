/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <memory>
#include <string>
#include <cstdlib>
#include <stdexcept>

#include "caf/config.hpp"
#include "caf/detail/demangle.hpp"

#if defined(CAF_CLANG) || defined(CAF_GCC)
# include <cxxabi.h>
# include <stdlib.h>
#endif

namespace caf {
namespace detail {

namespace {

// filter unnecessary characters from undecorated cstr
std::string filter_whitespaces(const char* cstr, size_t size = 0) {
  std::string result;
  if (size > 0) {
    result.reserve(size);
  }
  char c = *cstr;
  while (c != '\0') {
    if (c == ' ') {
      char previous_c = result.empty() ? ' ' : *(result.rbegin());
      for (c = *++cstr; c == ' '; c = *++cstr) {
        // scan for next non-space character
      }
      if (c != '\0') {
        // skip whitespace unless it separates two alphanumeric
        // characters (such as in "unsigned int")
        if (isalnum(c) && isalnum(previous_c)) {
          result += ' ';
          result += c;
        } else {
          result += c;
        }
        c = *++cstr;
      }
    } else {
      result += c;
      c = *++cstr;
    }
  }
  return result;
}

} // namespace <anonymous>

#if defined(CAF_CLANG) || defined(CAF_GCC)

  std::string demangle(const char* decorated) {
    using std::string;
    size_t size;
    int status;
    using uptr = std::unique_ptr<char, void (*)(void*)>;
    uptr undecorated{abi::__cxa_demangle(decorated, nullptr, &size, &status),
                     std::free};
    if (status != 0) {
      string error_msg = "Could not demangle type name ";
      error_msg += decorated;
      throw std::logic_error(error_msg);
    }
    // the undecorated typeid name
    string result = filter_whitespaces(undecorated.get(), size);
#   ifdef CAF_CLANG
      // replace "std::__1::" with "std::" (fixes strange clang names)
      string needle = "std::__1::";
      string fixed_string = "std::";
      for (auto pos = result.find(needle); pos != string::npos;
           pos = result.find(needle)) {
        result.replace(pos, needle.size(), fixed_string);
      }
#   endif
    return result;
  }

#elif defined(CAF_MSVC)

  string demangle(const char* decorated) {
    // on MSVC, name() returns a human-readable version, all we need
    // to do is to remove "struct" and "class" qualifiers
    // (handled in to_uniform_name)
    return filter_whitespaces(decorated);
  }

#else

# error "compiler or platform not supported"

#endif

std::string demangle(const std::type_info& tinf) {
  return demangle(tinf.name());
}

} // namespace detail
} // namespace caf
