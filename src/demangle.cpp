/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#include <memory>
#include <string>
#include <cstdlib>
#include <stdexcept>

#include "cppa/config.hpp"
#include "cppa/detail/demangle.hpp"

#if defined(CPPA_CLANG) || defined(CPPA_GCC)
#   include <cxxabi.h>
#   include <stdlib.h>
#endif

namespace cppa {
namespace detail {

namespace {

// filter unnecessary characters from undecorated cstr
std::string filter_whitespaces(const char* cstr, size_t size = 0) {
    std::string result;
    if (size > 0) result.reserve(size);
    char c = *cstr;
    while (c != '\0') {
        if (c == ' ') {
            char previous_c = result.empty() ? ' ' : *(result.rbegin());
            // get next non-space character
            for (c = *++cstr; c == ' '; c = *++cstr) {
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

#if defined(CPPA_CLANG) || defined(CPPA_GCC)

    std::string demangle(const char* decorated) {
        using std::string;
        size_t size;
        int status;
        std::unique_ptr<char, void (*)(void*)> undecorated{
            abi::__cxa_demangle(decorated, nullptr, &size, &status), std::free};
        if (status != 0) {
            string error_msg = "Could not demangle type name ";
            error_msg += decorated;
            throw std::logic_error(error_msg);
        }
        // the undecorated typeid name
        string result = filter_whitespaces(undecorated.get(), size);
#       ifdef CPPA_CLANG
        // replace "std::__1::" with "std::" (fixes strange clang names)
        string needle = "std::__1::";
        string fixed_string = "std::";
        for (auto pos = result.find(needle); pos != string::npos;
             pos = result.find(needle)) {
            result.replace(pos, needle.size(), fixed_string);
        }
#       endif
        return result;
    }

#elif defined(CPPA_MSVC)

    string demangle(const char* decorated) {
        // on MSVC, name() returns a human-readable version, all we need
        // to do is to remove "struct" and "class" qualifiers
        // (handled in to_uniform_name)
        return filter_whitespaces(decorated);
    }

#else
#   error "compiler or platform not supported"
#endif

std::string demangle(const std::type_info& tinf) {
    return demangle(tinf.name());
}

} // namespace detail
} // namespace cppa
