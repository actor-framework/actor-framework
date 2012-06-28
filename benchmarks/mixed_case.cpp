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

#include "utility.hpp"

#include "cppa/cppa.hpp"
#include "cppa/match.hpp"
#include "cppa/sb_actor.hpp"
#include "cppa/context_switching_actor.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::int64_t;
using std::uint64_t;

typedef std::vector<uint64_t> factors;

using namespace cppa;

constexpr uint64_t s_task_n = uint64_t(86028157)*329545133;
constexpr uint64_t s_factor1 = 86028157;
constexpr uint64_t s_factor2 = 329545133;

void check_factors(const factors& vec) {
    assert(vec.size() == 2);
    assert(vec[0] == s_factor1);
    assert(vec[1] == s_factor2);
#   ifdef NDEBUG
    static_cast<void>(vec);
#   endif
}

struct fsm_worker : sb_actor<fsm_worker> {
    actor_ptr mc;
    behavior init_state;
    fsm_worker(const actor_ptr& msgcollector) : mc(msgcollector) {
        init_state = (
            on<atom("calc"), uint64_t>() >> [=](uint64_t what) {
                send(mc, atom("result"), factorize(what));
            },
            on(atom("done")) >> [=]() {
                quit();
            }
        );
    }
};

struct fsm_chain_link : sb_actor<fsm_chain_link> {
    actor_ptr next;
    behavior init_state;
    fsm_chain_link(const actor_ptr& n) : next(n) {
        init_state = (
            on<atom("token"), int>() >> [=](int v) {
                next << std::move(last_dequeued());
                if (v == 0) quit();
            }
        );
    }
};

struct fsm_chain_master : sb_actor<fsm_chain_master> {
    int iteration;
    actor_ptr mc;
    actor_ptr next;
    actor_ptr worker;
    behavior init_state;
    void new_ring(int ring_size, int initial_token_value) {
        send(worker, atom("calc"), s_task_n);
        next = self;
        for (int i = 1; i < ring_size; ++i) {
            next = spawn<fsm_chain_link>(next);
        }
        send(next, atom("token"), initial_token_value);
    }
    fsm_chain_master(actor_ptr msgcollector) : iteration(0), mc(msgcollector) {
        init_state = (
            on(atom("init"), arg_match) >> [=](int rs, int itv, int n) {
                worker = spawn<fsm_worker>(msgcollector);
                iteration = 0;
                new_ring(rs, itv);
                become (
                    on(atom("token"), 0) >> [=]() {
                        if (++iteration < n) {
                            new_ring(rs, itv);
                        }
                        else {
                            send(worker, atom("done"));
                            send(mc, atom("masterdone"));
                            quit();
                        }
                    },
                    on<atom("token"), int>() >> [=](int v) {
                        send(next, atom("token"), v - 1);
                    }
                );
            }
        );
    }
};

struct fsm_supervisor : sb_actor<fsm_supervisor> {
    int left;
    behavior init_state;
    fsm_supervisor(int num_msgs) : left(num_msgs) {
        init_state = (
            on(atom("masterdone")) >> [=]() {
                if (--left == 0) quit();
            },
            on<atom("result"), factors>() >> [=](const factors& vec) {
                check_factors(vec);
                if (--left == 0) quit();
            }
        );
    }
};

void chain_link(actor_ptr next) {
    bool done = false;
    do_receive (
        on<atom("token"), int>() >> [&](int v) {
            if (v == 0) {
                done = true;
            }
            next << std::move(self->last_dequeued());
        }
    )
    .until([&]() { return done == true; });
}

void worker_fun(actor_ptr msgcollector) {
    bool done = false;
    do_receive (
        on<atom("calc"), uint64_t>() >> [&](uint64_t what) {
            send(msgcollector, atom("result"), factorize(what));
        },
        on(atom("done")) >> [&]() {
            done = true;
        }
    )
    .until([&]() { return done == true; });
}

actor_ptr new_ring(actor_ptr next, int ring_size) {
    for (int i = 1; i < ring_size; ++i) next = spawn(chain_link, next);
    return next;
}

void chain_master(actor_ptr msgcollector) {
    auto worker = spawn(worker_fun, msgcollector);
    receive (
        on(atom("init"), arg_match) >> [&](int rs, int itv, int n) {
            int iteration = 0;
            auto next = new_ring(self, rs);
            send(next, atom("token"), itv);
            send(worker, atom("calc"), s_task_n);
            do_receive (
                on<atom("token"), int>() >> [&](int v) {
                    if (v == 0) {
                        if (++iteration < n) {
                            next = new_ring(self, rs);
                            send(next, atom("token"), itv);
                            send(worker, atom("calc"), s_task_n);
                        }
                    }
                    else {
                        send(next, atom("token"), v - 1);
                    }
                }
            )
            .until([&]() { return iteration == n; });
        }
    );
    send(msgcollector, atom("masterdone"));
    send(worker, atom("done"));
}

void supervisor(int num_msgs) {
    do_receive (
        on(atom("masterdone")) >> [&]() {
            --num_msgs;
        },
        on<atom("result"), factors>() >> [&](const factors& vec) {
            --num_msgs;
            check_factors(vec);
        }
    )
    .until([&]() { return num_msgs == 0; });
}

template<typename F>
void run_test(F spawn_impl,
              int num_rings, int ring_size,
              int initial_token_value, int repetitions) {
    std::vector<actor_ptr> masters; // of the universe
    // each master sends one masterdone message and one
    // factorization is calculated per repetition
    //auto supermaster = spawn(supervisor, num_rings+repetitions);
    for (int i = 0; i < num_rings; ++i) {
        masters.push_back(spawn_impl());
        send(masters.back(), atom("init"),
                             ring_size,
                             initial_token_value,
                             repetitions);
    }
    await_all_others_done();
}

void usage() {
    cout << "usage: mailbox_performance "
            "(stacked|event-based) (num rings) (ring size) "
            "(initial token value) (repetitions)"
         << endl
         << endl;
    exit(1);
}

enum mode_type { event_based, fiber_based };

option<int> _2i(const std::string& str) {
    char* endptr = nullptr;
    int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
    if (endptr == nullptr || *endptr != '\0') {
       return {};
    }
    return result;
}

int main(int argc, char** argv) {
    announce<factors>();
    if (argc != 6) usage();
    // skip argv[0] (app name)
    std::vector<std::string> args{argv + 1, argv + argc};
    match(args) (
        on(val<std::string>, _2i, _2i, _2i, _2i) >> [](const std::string& mode,
                                                       int num_rings,
                                                       int ring_size,
                                                       int initial_token_value,
                                                       int repetitions) {
            int num_msgs = num_rings + (num_rings * repetitions);
            if (mode == "event-based") {
                auto mc = spawn<fsm_supervisor>(num_msgs);
                run_test([&]() { return spawn<fsm_chain_master>(mc); },
                         num_rings, ring_size, initial_token_value, repetitions);
            }
            else if (mode == "stacked") {
                auto mc = spawn(supervisor, num_msgs);
                run_test([&]() { return spawn(chain_master, mc); },
                         num_rings, ring_size, initial_token_value, repetitions);
            }
            else {
                usage();
            }
        },
        others() >> usage
    );
    return 0;
}
