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


#include <iterator>

#include "test.hpp"
#include "cppa/intrusive/singly_linked_list.hpp"
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

typedef cppa::intrusive::singly_linked_list<iint> iint_list;
typedef cppa::intrusive::single_reader_queue<iint> iint_queue;

size_t test__intrusive_containers() {
    CPPA_TEST(test__intrusive_containers);
    iint_list ilist1;
    ilist1.push_back(new iint(1));
    ilist1.emplace_back(2);
    ilist1.push_back(new iint(3)); {
        iint_list tmp;
        tmp.push_back(new iint(4));
        tmp.push_back(new iint(5));
        ilist1.splice_after(ilist1.before_end(), std::move(tmp));
        CPPA_CHECK(tmp.empty());
    }
    int iarr1[] = { 1, 2, 3, 4, 5 };
    CPPA_CHECK((std::equal(begin(iarr1), end(iarr1), begin(ilist1))));
    CPPA_CHECK((std::equal(begin(ilist1), end(ilist1), begin(iarr1))));

    ilist1.push_front(new iint(0));
    auto i = ilist1.erase_after(ilist1.begin());
    // i points to second element
    CPPA_CHECK_EQUAL(i->value, 2);
    i = ilist1.insert_after(i, new iint(20));
    CPPA_CHECK_EQUAL(i->value, 20);
    int iarr2[] = { 0, 2, 20, 3, 4, 5 };
    CPPA_CHECK((std::equal(begin(iarr2), end(iarr2), begin(ilist1))));

    auto p = ilist1.take();
    CPPA_CHECK(ilist1.empty());
    auto ilist2 = iint_list::from(p);
    ilist2.emplace_front(1);                    // 1 0 2 20 3 4 5
    i = ilist2.erase_after(ilist2.begin());     // 1 2 20 3 4 5
    CPPA_CHECK_EQUAL(i->value, 2);
    ilist2.erase_after(i);                      // 1 2 3 4 5
    CPPA_CHECK((std::equal(begin(iarr1), end(iarr1), begin(ilist2))));

    // five elements + two dummies
    CPPA_CHECK_EQUAL(s_iint_instances, 7);

    ilist2.remove_if([](const iint& val) { return (val.value % 2) != 0; });

    // two elements + two dummies
    CPPA_CHECK_EQUAL(s_iint_instances, 4);

    int iarr3[] = { 2, 4 };
    CPPA_CHECK((std::equal(begin(iarr3), end(iarr3), begin(ilist2))));

    auto xy = ilist2.take_after(ilist2.before_begin());
    CPPA_CHECK_EQUAL(xy->value, 2);
    delete xy;

    ilist2.clear();
    // two dummies
    CPPA_CHECK_EQUAL(s_iint_instances, 2);
    CPPA_CHECK(ilist2.empty());

    cppa::intrusive::single_reader_queue<iint> q;
    q.push_back(new iint(1));
    q.push_back(new iint(2));
    q.push_back(new iint(3));

    auto x = q.pop();
    CPPA_CHECK_EQUAL(1, x->value);
    delete x;
    x = q.pop();
    CPPA_CHECK_EQUAL(2, x->value);
    delete x;
    x = q.pop();
    CPPA_CHECK_EQUAL(3, x->value);
    delete x;
    x = q.try_pop();
    CPPA_CHECK(x == nullptr);

    return CPPA_TEST_RESULT;
}
