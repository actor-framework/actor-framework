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
#include "cppa/cow_tuple.hpp"
#include "cppa/announce.hpp"

using std::cout;
using std::cerr;
using std::endl;

using std::list;
using std::string;
using std::int64_t;

using namespace cppa;

template<typename T>
T rd(const char* cstr) {
    char* endptr = nullptr;
    T result = static_cast<T>(strtol(cstr, &endptr, 10));
    if (endptr == nullptr || *endptr != '\0') {
        std::string errstr;
        errstr += "\"";
        errstr += cstr;
        errstr += "\" is not an integer";
        throw std::invalid_argument(errstr);
    }
    return result;
}

void usage() {
    cerr << "usage: matching (cow_tuple|object_array) {NUM_LOOPS}" << endl;
    exit(1);
}

int main(int argc, char** argv) {
    announce<list<int>>();
    if (argc != 3) usage();
    auto num_loops = rd<int64_t>(argv[2]);
    any_tuple m1;
    any_tuple m2;
    any_tuple m3;
    any_tuple m4;
    any_tuple m5;
    any_tuple m6;

    if (strcmp(argv[1], "cow_tuple") == 0) {
        m1 = make_cow_tuple(atom("msg1"), 0);
        m2 = make_cow_tuple(atom("msg2"), 0.0);
        m3 = make_cow_tuple(atom("msg3"), list<int>{0});
        m4 = make_cow_tuple(atom("msg4"), 0, "0");
        m5 = make_cow_tuple(atom("msg5"), 0, 0, 0);
        m6 = make_cow_tuple(atom("msg6"), 0, 0.0, "0");
    }
    else if (strcmp(argv[1], "object_array") == 0) {
        auto m1o = new detail::object_array;
        m1o->push_back(object::from(atom("msg1")));
        m1o->push_back(object::from(0));
        m1 = any_tuple{m1o};
        auto m2o = new detail::object_array;
        m2o->push_back(object::from(atom("msg2")));
        m2o->push_back(object::from(0.0));
        m2 = any_tuple{m2o};
        auto m3o = new detail::object_array;
        m3o->push_back(object::from(atom("msg3")));
        m3o->push_back(object::from(list<int>{0}));
        m3 = any_tuple{m3o};
        auto m4o = new detail::object_array;
        m4o->push_back(object::from(atom("msg4")));
        m4o->push_back(object::from(0));
        m4o->push_back(object::from(std::string("0")));
        m4 = any_tuple{m4o};
        auto m5o = new detail::object_array;
        m5o->push_back(object::from(atom("msg5")));
        m5o->push_back(object::from(0));
        m5o->push_back(object::from(0));
        m5o->push_back(object::from(0));
        m5 = any_tuple{m5o};
        auto m6o = new detail::object_array;
        m6o->push_back(object::from(atom("msg6")));
        m6o->push_back(object::from(0));
        m6o->push_back(object::from(0.0));
        m6o->push_back(object::from(std::string("0")));
        m6 = any_tuple{m6o};
    }
    else {
        usage();
    }
    int64_t m1matched = 0;
    int64_t m2matched = 0;
    int64_t m3matched = 0;
    int64_t m4matched = 0;
    int64_t m5matched = 0;
    int64_t m6matched = 0;
    auto part_fun = (
        on<atom("msg1"), int>() >> [&]() { ++m1matched; },
        on<atom("msg2"), double>() >> [&]() { ++m2matched; },
        on<atom("msg3"), list<int> >() >> [&]() { ++m3matched; },
        on<atom("msg4"), int, string>() >> [&]() { ++m4matched; },
        on<atom("msg5"), int, int, int>() >> [&]() { ++m5matched; },
        on<atom("msg6"), int, double, string>() >> [&]() { ++m6matched; }
    );
    for (int64_t i = 0; i < num_loops; ++i) {
        part_fun(m1);
        part_fun(m2);
        part_fun(m3);
        part_fun(m4);
        part_fun(m5);
        part_fun(m6);
    }
    assert(m1matched == num_loops);
    assert(m2matched == num_loops);
    assert(m3matched == num_loops);
    assert(m4matched == num_loops);
    assert(m5matched == num_loops);
    assert(m6matched == num_loops);
}
