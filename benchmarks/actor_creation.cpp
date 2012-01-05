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


#include <cstdlib>
#include <cstring>
#include <iostream>
#include "cppa/cppa.hpp"
#include "cppa/fsm_actor.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa;

struct testee : event_based_actor
{
    int m_x;
    testee(int x) : m_x(x)
    {
    }
    void init()
    {
        if (m_x > 0)
        {
            spawn(new testee(m_x - 1));
            spawn(new testee(m_x - 1));
        }
    }
};

void cr_stacked_actor(int x)
{
    if (x > 0)
    {
        spawn(cr_stacked_actor, x - 1);
        spawn(cr_stacked_actor, x - 1);
    }
}

void usage()
{
    cout << "usage: actor_creation (stacked|event-based) POW" << endl
         << "       creates 2^POW actors" << endl
         << endl;
}

int main(int argc, char** argv)
{
    if (argc == 3)
    {
        char* endptr = nullptr;
        int num = static_cast<int>(strtol(argv[2], &endptr, 10));
        if (endptr == nullptr || *endptr != '\0')
        {
            cerr << "\"" << argv[2] << "\" is not an integer" << endl;
            return 1;
        }
        if (strcmp(argv[1], "stacked") == 0)
        {
            spawn(cr_stacked_actor, num);
        }
        else if (strcmp(argv[1], "event-based") == 0)
        {
            spawn(new testee(num));
        }
        else
        {
            usage();
            return 1;
        }
        await_all_others_done();
    }
    else
    {
        usage();
        return 1;
    }
    return 0;
}
