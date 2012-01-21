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
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#include <list>
#include <string>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "cppa/on.hpp"
#include "cppa/atom.hpp"
#include "cppa/match.hpp"
#include "cppa/tuple.hpp"
#include "cppa/announce.hpp"

using std::cout;
using std::cerr;
using std::endl;

using std::list;
using std::string;
using std::int64_t;

using namespace cppa;

template<typename T>
T rd(char const* cstr)
{
    char* endptr = nullptr;
    T result = static_cast<T>(strtol(cstr, &endptr, 10));
    if (endptr == nullptr || *endptr != '\0')
    {
        std::string errstr;
        errstr += "\"";
        errstr += cstr;
        errstr += "\" is not an integer";
        throw std::invalid_argument(errstr);
    }
    return result;
}

int main(int argc, char** argv)
{
    announce<list<int>>();
    if (argc != 2)
    {
        cerr << "usage: matching {NUM_LOOPS}" << endl;
        return 1;
    }
    auto num_loops = rd<int64_t>(argv[1]);
    any_tuple m1 = make_tuple(atom("msg1"), 0);
    any_tuple m2 = make_tuple(atom("msg2"), 0.0);
    any_tuple m3 = cppa::make_tuple(atom("msg3"), list<int>{0});
    any_tuple m4 = make_tuple(atom("msg4"), 0, "0");
    any_tuple m5 = make_tuple(atom("msg5"), 0, 0, 0);
    any_tuple m6 = make_tuple(atom("msg6"), 0, 0.0, "0");
    int64_t m1matched = 0;
    int64_t m2matched = 0;
    int64_t m3matched = 0;
    int64_t m4matched = 0;
    int64_t m5matched = 0;
    int64_t m6matched = 0;
    auto part_fun =
    (
        on<atom("msg1"), int>() >> [&]() { ++m1matched; },
        on<atom("msg2"), double>() >> [&]() { ++m2matched; },
        on<atom("msg3"), list<int> >() >> [&]() { ++m3matched; },
        on<atom("msg4"), int, string>() >> [&]() { ++m4matched; },
        on<atom("msg5"), int, int, int>() >> [&]() { ++m5matched; },
        on<atom("msg6"), int, double, string>() >> [&]() { ++m6matched; }
    );
    for (int64_t i = 0; i < num_loops; ++i)
    {
        part_fun(m1);
        part_fun(m2);
        part_fun(m3);
        part_fun(m4);
        part_fun(m5);
        part_fun(m6);
    }
}
