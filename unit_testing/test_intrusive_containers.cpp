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
 * Copyright (C) 2011-2013                                                    *
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


#include <iterator>

#include "test.hpp"
#include "cppa/intrusive/single_reader_queue.hpp"

using std::begin;
using std::end;

namespace { size_t s_iint_instances = 0; }

struct iint {
    iint* next;
    int value;
    inline iint(int val = 0) : next(nullptr), value(val) { ++s_iint_instances; }
    ~iint() { --s_iint_instances; }
};

inline bool operator==(const iint& lhs, const iint& rhs) {
    return lhs.value == rhs.value;
}

inline bool operator==(const iint& lhs, int rhs) {
    return lhs.value == rhs;
}

inline bool operator==(int lhs, const iint& rhs) {
    return lhs == rhs.value;
}

typedef cppa::intrusive::single_reader_queue<iint> iint_queue;

int main() {
    CPPA_TEST(test__intrusive_containers);

    cppa::intrusive::single_reader_queue<iint> q;
    q.enqueue(new iint(1));
    q.enqueue(new iint(2));
    q.enqueue(new iint(3));

    CPPA_CHECK_EQUAL(3, s_iint_instances);

    auto x = q.try_pop();
    CPPA_CHECK_EQUAL(x->value, 1);
    delete x;
    x = q.try_pop();
    CPPA_CHECK_EQUAL(x->value, 2);
    delete x;
    x = q.try_pop();
    CPPA_CHECK_EQUAL(x->value, 3);
    delete x;
    x = q.try_pop();
    CPPA_CHECK(x == nullptr);

    return CPPA_TEST_RESULT();
}
