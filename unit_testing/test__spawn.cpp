//#define CPPA_VERBOSE_CHECK

#include <stack>
#include <chrono>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/fsm_actor.hpp"
#include "cppa/to_string.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/event_based_actor.hpp"
#include "cppa/util/callable_trait.hpp"

using std::cerr;
using std::cout;
using std::endl;

using namespace cppa;

#if (__GNUC__ >= 4) && (__GNUC_MINOR__ >= 7)

struct simple_mirror : fsm_actor<simple_mirror> {

    behavior init_state = (
        others() >> []() {
            self->last_sender() << self->last_dequeued();
        }
    );

};

#else

struct simple_mirror : event_based_actor {

    void init() {
        become (
            others() >> []() {
                self->last_sender() << self->last_dequeued();
            }
        );
    }

};

#endif

// GCC 4.7 supports non-static member initialization
#if 0 //(__GNUC__ >= 4) && (__GNUC_MINOR__ >= 7)

struct event_testee : public fsm_actor<event_testee> {

    behavior wait4string = (
        on<std::string>() >> [=]() {
            become(&wait4int);
        },
        on<atom("get_state")>() >> [=]() {
            reply("wait4string");
        }
    );

    behavior wait4float = (
        on<float>() >> [=]() {
            become(&wait4string);
        },
        on<atom("get_state")>() >> [=]() {
            reply("wait4float");
        }
    );

    behavior wait4int = (
        on<int>() >> [=]() {
            become(&wait4float);
        },
        on<atom("get_state")>() >> [=]() {
            reply("wait4int");
        }
    );

    behavior& init_state = wait4int;

};

#else

class event_testee : public fsm_actor<event_testee> {

    friend class fsm_actor<event_testee>;

    behavior wait4string;
    behavior wait4float;
    behavior wait4int;

    behavior& init_state;

 public:

    event_testee() : init_state(wait4int) {
        wait4string = (
            on<std::string>() >> [=]() {
                become(&wait4int);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4string");
            }
        );

        wait4float = (
            on<float>() >> [=]() {
                become(&wait4string);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4float");
            }
        );

        wait4int = (
            on<int>() >> [=]() {
                become(&wait4float);
            },
            on<atom("get_state")>() >> [=]() {
                reply("wait4int");
            }
        );
    }

};

#endif

// quits after 5 timeouts
event_based_actor* event_testee2() {
    struct impl : fsm_actor<impl> {
        behavior wait4timeout(int remaining) {
            return (
                after(std::chrono::milliseconds(50)) >> [=]() {
                    if (remaining == 1) quit();
                    else become(wait4timeout(remaining - 1));
                }
            );
        }

        behavior init_state;

        impl() : init_state(wait4timeout(5)) { }
    };
    return new impl;
}

struct chopstick : public fsm_actor<chopstick> {

    behavior taken_by(actor_ptr hakker) {
        return (
            on<atom("take")>() >> [=]() {
                reply(atom("busy"));
            },
            on(atom("put"), hakker) >> [=]() {
                become(&init_state);
            },
            on(atom("break")) >> [=]() {
                quit();
            }
        );
    }

    behavior init_state;

    chopstick() {
        init_state = (
            on(atom("take"), arg_match) >> [=](actor_ptr hakker) {
                become(taken_by(hakker));
                reply(atom("taken"));
            },
            on(atom("break")) >> [=]() {
                quit();
            },
            others() >> [=]() {
            }
        );
    }

};

class testee_actor {

    void wait4string() {
        bool string_received = false;
        do_receive (
            on<std::string>() >> [&]() {
                string_received = true;
            },
            on<atom("get_state")>() >> [&]() {
                reply("wait4string");
            }
        )
        .until(gref(string_received));
    }

    void wait4float() {
        bool float_received = false;
        do_receive (
            on<float>() >> [&]() {
                float_received = true;
                wait4string();
            },
            on<atom("get_state")>() >> [&]() {
                reply("wait4float");
            }
        )
        .until(gref(float_received));
    }

 public:

    void operator()() {
        receive_loop (
            on<int>() >> [&]() {
                wait4float();
            },
            on<atom("get_state")>() >> [&]() {
                reply("wait4int");
            }
        );
    }

};

// receives one timeout and quits
void testee1() {
    receive (
        after(std::chrono::milliseconds(10)) >> []() { }
    );
}

void testee2(actor_ptr other) {
    self->link_to(other);
    send(other, std::uint32_t(1));
    receive_loop (
        on<std::uint32_t>() >> [](std::uint32_t sleep_time) {
            // "sleep" for sleep_time milliseconds
            receive(after(std::chrono::milliseconds(sleep_time)) >> []() {});
            //reply(sleep_time * 2);
        }
    );
}

void testee3(actor_ptr parent) {
    // test a delayed_send / delayed_reply based loop
    delayed_send(self, std::chrono::milliseconds(50), atom("Poll"));
    int polls = 0;
    receive_for(polls, 5) (
        on(atom("Poll")) >> [&]() {
            if (polls < 4) {
                delayed_reply(std::chrono::milliseconds(50), atom("Poll"));
            }
            send(parent, atom("Push"), polls);
        }
    );
}

void echo_actor() {
    receive (
        others() >> []() {
            self->last_sender() << self->last_dequeued();
        }
    );
}

template<class Testee>
std::string behavior_test(actor_ptr et) {
    std::string result;
    std::string testee_name = detail::to_uniform_name(typeid(Testee));
    send(et, 1);
    send(et, 2);
    send(et, 3);
    send(et, .1f);
    send(et, "hello " + testee_name);
    send(et, .2f);
    send(et, .3f);
    send(et, "hello again " + testee_name);
    send(et, "goodbye " + testee_name);
    send(et, atom("get_state"));
    receive (
        on_arg_match >> [&](const std::string& str) {
            result = str;
        },
        after(std::chrono::minutes(1)) >> [&]() {
        //after(std::chrono::seconds(2)) >> [&]() {
            throw std::runtime_error(testee_name + " does not reply");
        }
    );
    send(et, atom("EXIT"), exit_reason::user_defined);
    await_all_others_done();
    return result;
}

template<class MatchExpr>
class actor_template {

    MatchExpr m_expr;

 public:

    actor_template(MatchExpr me) : m_expr(std::move(me)) { }

    actor_ptr spawn() const {
        struct impl : fsm_actor<impl> {
            behavior init_state;
            impl(const MatchExpr& mx) : init_state(mx.as_partial_function()) {
            }
        };
        return cppa::spawn(new impl{m_expr});
    }

};

template<typename... Args>
auto actor_prototype(const Args&... args) -> actor_template<decltype(mexpr_concat(args...))> {
    return {mexpr_concat(args...)};
}

template<typename InitFun, typename CleanupFun, typename... Members>
class simple_event_based_actor_impl : public event_based_actor {

 public:

    template<typename... Args>
    simple_event_based_actor_impl(InitFun fun, CleanupFun cfun, Args&&... args)
    : m_init(std::move(fun)), m_cleanup(std::move(cfun))
    , m_members(std::forward<Args>(args)...) { }

    void init() { apply(m_init); }

    void on_exit() { m_cleanup(); }

 private:

    InitFun m_init;
    CleanupFun m_cleanup;
    detail::tdata<Members...> m_members;

    template<typename F>
    void apply(F& f, typename std::add_pointer<Members>::type... args) {
        f(args...);
    }

    template<typename F, typename... Args>
    void apply(F& f, Args... args) {
        apply(f, args..., &get_ref<sizeof...(Args)>(m_members));
    }

};

template<typename InitFun, typename CleanupFun, typename... Members>
class simple_event_based_actor_factory {

 public:

    typedef simple_event_based_actor_impl<InitFun, CleanupFun, Members...> impl;

    simple_event_based_actor_factory(InitFun fun, CleanupFun cfun)
    : m_init(std::move(fun)), m_cleanup(std::move(cfun)) { }

    template<typename... Args>
    actor_ptr spawn(Args&&... args) {
        return cppa::spawn(new impl(m_init, m_cleanup, std::forward<Args>(args)...));
    }

 private:

    InitFun m_init;
    CleanupFun m_cleanup;

};

template<typename InitFun, typename CleanupFun, class TypeList>
struct actor_tpl_from_type_list;

template<typename InitFun, typename CleanupFun, typename... Ts>
struct actor_tpl_from_type_list<InitFun, CleanupFun, util::type_list<Ts...> > {
    typedef simple_event_based_actor_factory<InitFun, CleanupFun, Ts...> type;
};

template<typename Init, typename Cleanup>
struct actor_tpl_from_fun {
    typedef typename util::get_arg_types<Init>::types arg_types;
    typedef typename util::get_arg_types<Cleanup>::types arg_types2;
    static_assert(util::tl_forall<arg_types, std::is_pointer>::value,
                  "First functor takes non-pointer arguments");
    static_assert(   std::is_same<arg_types, arg_types2>::value
                  || std::is_same<util::type_list<>, arg_types2>::value,
                  "Second functor must provide either the same signature "
                  " as the first one or take zero arguments");
    typedef typename util::tl_map<arg_types, std::remove_pointer>::type mems;
    typedef typename actor_tpl_from_type_list<Init, Cleanup, mems>::type type;
};

void dummy_function() { }

struct factory {

    template<typename Fun>
    static inline typename actor_tpl_from_fun<Fun, void (*)()>::type event_based(Fun fun) {
        return {std::move(fun), dummy_function};
    }

};

size_t test__spawn() {
    using std::string;
    CPPA_TEST(test__spawn);

    CPPA_IF_VERBOSE(cout << "test send() ... " << std::flush);
    send(self, 1, 2, 3);
    receive(on(1, 2, 3) >> []() { });
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "test receive with zero timeout ... " << std::flush);
    receive (
        others() >> []() {
            cerr << "WTF?? received: " << to_string(self->last_dequeued())
                 << endl;
        },
        after(std::chrono::seconds(0)) >> []() {
            // mailbox empty
        }
    );
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "test echo actor ... " << std::flush);
    auto mecho = spawn(echo_actor);
    send(mecho, "hello echo");
    receive(on("hello echo") >> []() { });
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    auto mirror = spawn(new simple_mirror);

    CPPA_IF_VERBOSE(cout << "test mirror ... " << std::flush);
    send(mirror, "hello mirror");
    receive(on("hello mirror") >> []() { });
    send(mirror, atom("EXIT"), exit_reason::user_defined);
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    auto svec = std::make_shared<std::vector<string> >();
    auto avec = actor_prototype (
        on(atom("push_back"), arg_match) >> [svec](const string& str) {
            svec->push_back(str);
        },
        on(atom("get")) >> [svec]() {
            reply(*svec);
        }
    ).spawn();

    send(avec, atom("push_back"), "hello");
    send(avec, atom("push_back"), " world");
    send(avec, atom("get"));
    send(avec, atom("EXIT"), exit_reason::user_defined);
    receive (
        on_arg_match >> [&](const std::vector<string>& vec) {
            if (vec.size() == 2)
            {
                CPPA_CHECK_EQUAL("hello world", vec.front() + vec.back());
            }
        }
    );

    CPPA_IF_VERBOSE(cout << "test delayed_send() ... " << std::flush);
    delayed_send(self, std::chrono::seconds(1), 1, 2, 3);
    receive(on(1, 2, 3) >> []() { });
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "test timeout ... " << std::flush);
    receive(after(std::chrono::seconds(1)) >> []() { });
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "testee1 ... " << std::flush);
    spawn(testee1);
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "event_testee2 ... " << std::flush);
    spawn(event_testee2());
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "chopstick ... " << std::flush);
    auto cstk = spawn(new chopstick);
    send(cstk, atom("take"), self);
    receive (
        on(atom("taken")) >> [&]() {
            send(cstk, atom("put"), self);
            send(cstk, atom("break"));
        }
    );
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_IF_VERBOSE(cout << "test event-based factory ... " << std::flush);
    auto factory = factory::event_based([&](int* i, float*, std::string*) {
        self->become (
            on(atom("get_int")) >> [i]() {
                reply(*i);
            },
            on(atom("set_int"), arg_match) >> [i](int new_value) {
                *i = new_value;
            },
            on(atom("done")) >> []() {
                self->quit();
            }
        );
    });
    auto foobaz_actor = factory.spawn(23);
    send(foobaz_actor, atom("get_int"));
    send(foobaz_actor, atom("set_int"), 42);
    send(foobaz_actor, atom("get_int"));
    send(foobaz_actor, atom("done"));
    receive (
        on_arg_match >> [&](int value) {
            CPPA_CHECK_EQUAL(23, value);
        }
    );
    receive (
        on_arg_match >> [&](int value) {
            CPPA_CHECK_EQUAL(42, value);
        }
    );
    await_all_others_done();
    CPPA_IF_VERBOSE(cout << "ok" << endl);

    CPPA_CHECK_EQUAL(behavior_test<testee_actor>(spawn(testee_actor{})), "wait4int");
    CPPA_CHECK_EQUAL(behavior_test<event_testee>(spawn(new event_testee)), "wait4int");

    // create 20,000 actors linked to one single actor
    // and kill them all through killing the link
    auto twenty_thousand = spawn([]() {
        for (int i = 0; i < 20000; ++i) {
            self->link_to(spawn(new event_testee));
        }
        receive_loop (
            others() >> []() {
                cout << "wtf? => " << to_string(self->last_dequeued()) << endl;
            }
        );
    });
    send(twenty_thousand, atom("EXIT"), exit_reason::user_defined);
    await_all_others_done();
    self->trap_exit(true);
    auto ping_actor = spawn(ping, 10);
    auto pong_actor = spawn(pong, ping_actor);
    self->monitor(pong_actor);
    self->monitor(ping_actor);
    self->link_to(pong_actor);
    int i = 0;
    int flags = 0;
    delayed_send(self, std::chrono::seconds(1), atom("FooBar"));
    // wait for DOWN and EXIT messages of pong
    receive_for(i, 4) (
        on<atom("EXIT"), std::uint32_t>() >> [&](std::uint32_t reason) {
            CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            CPPA_CHECK(self->last_sender() == pong_actor);
            flags |= 0x01;
        },
        on<atom("DOWN"), std::uint32_t>() >> [&](std::uint32_t reason) {
            auto who = self->last_sender();
            if (who == pong_actor) {
                flags |= 0x02;
                CPPA_CHECK_EQUAL(reason, exit_reason::user_defined);
            }
            else if (who == ping_actor) {
                flags |= 0x04;
                CPPA_CHECK_EQUAL(reason, exit_reason::normal);
            }
        },
        on<atom("FooBar")>() >> [&]() {
            flags |= 0x08;
        },
        others() >> [&]() {
            CPPA_ERROR("unexpected message: " << to_string(self->last_dequeued()));
        },
        after(std::chrono::seconds(5)) >> [&]() {
            CPPA_ERROR("timeout in file " << __FILE__ << " in line " << __LINE__);
        }
    );
    // wait for termination of all spawned actors
    await_all_others_done();
    CPPA_CHECK_EQUAL(0x0F, flags);
    // verify pong messages
    CPPA_CHECK_EQUAL(10, pongs());
    return CPPA_TEST_RESULT;

    /****** TODO

    int add_fun(int, int);
    int sub_fun(int, int);

    ...

    class math {
     public:
        int add_fun(int, int);
        int sub_fun(int, int);
    };

    ...

    auto factory1 = actor_prototype (
        on<atom("add"), int, int>().reply_with(atom("result"), add_fun),
        on<atom("sub"), int, int>().reply_with(atom("result"), add_fun)
    );
    auto a1 = factory1.spawn();

    ...

    auto factory2 = actor_facade<math> (
        on<atom("add"), int, int>().reply_with(atom("result"), &math::add_fun), <-- possible?
        on<atom("sub"), int, int>().reply_with(atom("result"), &math::sub_fun)
    );
    auto a2 = factory2.spawn();

    */

}
