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


#include <sstream>

#include "cppa/string_algorithms.hpp"

using std::string;
using std::vector;

namespace cppa {

void split(vector<string>& result, const string& str,
           const string& delims, bool keep_all) {
    size_t pos = 0;
    size_t prev = 0;
    while ((pos = str.find_first_of(delims, prev)) != string::npos) {
        if (pos > prev) {
            auto substr = str.substr(prev, pos - prev);
            if (!substr.empty() || keep_all) result.push_back(std::move(substr));
        }
        prev = pos + 1;
    }
    if (prev < str.size()) {
        result.push_back(str.substr(prev, string::npos));
    }
}

} // namespace cppa
