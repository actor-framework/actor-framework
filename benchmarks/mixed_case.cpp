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


#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "cppa/cppa.hpp"
#include "cppa/fsm_actor.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::int64_t;
using std::uint64_t;

typedef std::vector<uint64_t>;

using namespace cppa;

constexpr int s_num_messages = 1000;
constexpr uint64_t s_task_n = uint64_t(86028157)*329545133;
constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;

factors factorize(uint64_t n)
{
    factors result;
    uint64_t d = 2;
    if (n <= 3)
    {
        result.push_back(n);
        return std::move(result);
    }
    // while the factor being tested
    // is lower than the number to factorize
    while(d < n)
    {
        // if valid prime
        if((n % d) == 0)
        {
            result.push_back(d);
            n /= d;
        }
        else
        {
            d = (d == 2) ? 3 : (d + 2);
        }
    }
    result.push_back(d);
    return std::move(result);
}

struct fsm_worker : fsm_actor<fsm_worker>
{
    behavior init_state;
    fsm_worker()
    {
        init_state =
        (
            on<atom("calc"), uint64_t>() >> [=](uint64_t what)
            {
                reply(atom("result"), factorize(what));
            }
        );
    }
};

struct fsm_chain_link : fsm_actor<fsm_chain_link>
{
    actor_ptr next;
    behavior init_state;
    fsm_chain_link(actor_ptr const& n) : next(n)
    {
        init_state =
        (
            on<atom("token"), int>() >> [=](int v)
            {
                send(next, atom("token"), v);
                if (v == 0)
                {
                    quit(exit_reason::normal);
                }
            }
        );
    }
};

struct fsm_chain_master : fsm_actor<fsm_chain_master>
{
    int iteration;
    actor_ptr next;
    actor_ptr worker;
    behavior init_state;
    void new_ring(int ring_size)
    {
        send(worker, atom("calc"), s_task_n);
        next = self;
        for (int i = 1; i < ring_size; ++i)
        {
            next = spawn(new fsm_chain_link(next));
        }
        send(next, atom("token"), s_num_messages);
    }
    fsm_chain_master() : iteration(0)
    {
        worker = spawn(new fsm_worker);
        init_state =
        (
            on<atom("init"), int, int>() >> [=](int ring_size, int repetitions)
            {
                iteration = 0;
                new_ring(ring_size);
                become
                (
                    on<atom("token"), int>() >> [=](int v)
                    {
                        if (v == 0)
                        {
                            if (++iteration < repetitions)
                            {
                                new_ring(ring_size);
                            }
                            else
                            {
                                send(worker, atom(":Exit"),
                                             exit_reason::user_defined);
                                quit(exit_reason::normal);
                            }
                        }
                        else
                        {
                            send(next, atom("token"), v - 1);
                        }
                    },
                    on<atom("result"), factors>() >> [](factors const& vec)
                    {
                        assert(vec.size() == 2);
                        assert(vec[0] == s_factor1);
                        assert(vec[1] == s_factor2);
                    }
                );
            }
        );
    }
};
void tst();

void chain_link(actor_ptr next)
{
    receive_loop
    (
        on<atom("token"), int>() >> [&](int v)
        {
            send(next, atom("token"), v);
            if (v == 0)
            {
                quit(exit_reason::normal);
            }
        }
    );
}

void chain_master()
{
    auto worker = spawn([]()
    {
        receive_loop
        (
            on<atom("calc"), uint64_t>() >> [=](uint64_t what)
            {
                reply(atom("result"), factorize(what));
            }
        );
    });
    actor_ptr next;
    auto new_ring = [&](int ring_size)
    {
        send(worker, atom("calc"), s_task_n);
        next = self;
        for (int i = 1; i < ring_size; ++i)
        {
            next = spawn(chain_link, next);
        }
        send(next, atom("token"), s_num_messages);
    };
    receive
    (
        on<atom("init"), int, int>() >> [&](int ring_size, int repetitions)
        {
            int iteration = 0;
            new_ring(ring_size);
            do_receive
            (
                on<atom("token"), int>() >> [&](int v)
                {
                    if (v == 0)
                    {
                        if (++iteration < repetitions)
                        {
                            new_ring(ring_size);
                        }
                    }
                    else
                    {
                        send(next, atom("token"), v - 1);
                    }
                },
                on<atom("result"), factors>() >> [](factors const& vec)
                {
                    assert(vec.size() == 2);
                    assert(vec[0] == s_factor1);
                    assert(vec[1] == s_factor2);
                }
            )
            .until([&]() { return iteration == repetitions; });
        }
    );
    send(worker, atom(":Exit"), exit_reason::user_defined);
}

template<typename F>
void run_test(F&& spawn_impl)
{
    std::vector<actor_ptr> masters; // of the universe
    for (int i = 0; i < 10; ++i)
    {
        masters.push_back(spawn_impl());
        send(masters.back(), atom("init"), 50, 20);
    }
    await_all_others_done();
}

int main(int argc, char** argv)
{
    announce<factors>();
    if (argc == 2)
    {
        if (strcmp(argv[1], "event-based") == 0)
        {
            run_test([]() { return spawn(new fsm_chain_master); });
            return 0;
        }
        else if (strcmp(argv[1], "stacked") == 0)
        {
            run_test([]() { return spawn(chain_master); });
            return 0;
        }
    }
    cerr << "usage: mixed_case (stacked|event-based)" << endl;
    return 1;
}
