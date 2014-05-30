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
    CPPA_TEST(test_intrusive_containers);

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
