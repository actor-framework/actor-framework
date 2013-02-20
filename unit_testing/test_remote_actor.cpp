#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>
#include <functional>

#include "test.hpp"
#include "ping_pong.hpp"
#include "cppa/cppa.hpp"
#include "cppa/logging.hpp"
#include "cppa/exception.hpp"

using namespace std;
using namespace cppa;

namespace {

typedef std::pair<std::string, std::string> string_pair;

typedef vector<actor_ptr> actor_vector;

vector<string_pair> get_kv_pairs(int argc, char** argv, int begin = 1) {
    vector<string_pair> result;
    for (int i = begin; i < argc; ++i) {
        auto vec = split(argv[i], '=');
        auto eq_fun = [&](const string_pair& p) { return p.first == vec[0]; };
        if (vec.size() != 2) {
            CPPA_PRINTERR("\"" << argv[i] << "\" is not a key-value pair");
        }
        else if (any_of(result.begin(), result.end(), eq_fun)) {
            CPPA_PRINTERR("key \"" << vec[0] << "\" is already defined");
        }
        else {
            result.emplace_back(vec[0], vec[1]);
        }
    }
    return result;
}

struct reflector : public event_based_actor {
    void init() {
        become (
            others() >> [=] {
                reply_tuple(last_dequeued());
                quit();
            }
        );
    }
};

struct replier : public event_based_actor {
    void init() {
        become (
            others() >> [=] {
                reply(42);
                quit();
            }
        );
    }
};

// receive seven reply messages (2 local, 5 remote)
void spawn5_server(actor_ptr client, bool inverted) {
    auto default_case = others() >> CPPA_UNEXPECTED_MSG_CB();
    group_ptr grp;
    if (!inverted) {
        grp = group::get("local", "foobar");
    }
    else {
        send(client, atom("GetGroup"));
        receive (
            on_arg_match >> [&](const group_ptr& remote_group) {
                grp = remote_group;
            }
        );
    }
    spawn_in_group<reflector>(grp);
    spawn_in_group<reflector>(grp);
    receive_response (sync_send(client, atom("Spawn5"), grp)) (
        on(atom("ok"), arg_match) >> [&](const actor_vector& vec) {
            send(grp, "Hello reflectors!", 5.0);
            if (vec.size() != 5) {
                CPPA_PRINT("remote client did not spawn five reflectors!");
            }
            for (auto& a : vec) {
                self->monitor(a);
            }
        },
        default_case,
        after(chrono::seconds(10)) >> [&] {
            throw runtime_error("timeout");
        }
    );
    CPPA_PRINT("wait for reflected messages");
    // receive seven reply messages (2 local, 5 remote)
    int x = 0;
    receive_for(x, 7) (
        on("Hello reflectors!", 5.0) >> [] { }
    );
    CPPA_PRINT("wait for DOWN messages");
    // wait for DOWN messages
    {int i = 0; receive_for(i, 5) (
        on(atom("DOWN"), arg_match) >> [](std::uint32_t reason) {
            if (reason != exit_reason::normal) {
                CPPA_PRINT("reflector exited for non-normal exit reason!");
            }
        },
        default_case,
        after(chrono::seconds(2)) >> [&] {
            i = 4;
            CPPA_PRINTERR("timeout while waiting for DOWN messages!");
        }
    );}
    // wait for locally spawned reflectors
    await_all_others_done();
    send(client, atom("Spawn5Done"));
}

void spawn5_client() {
    bool spawned_reflectors = false;
    do_receive (
        on(atom("Spawn5"), arg_match) >> [&](const group_ptr& grp) {
            actor_vector vec;
            for (int i = 0; i < 5; ++i) {
                vec.push_back(spawn_in_group<reflector>(grp));
            }
            reply(atom("ok"), std::move(vec));
            spawned_reflectors = true;
        },
        on(atom("GetGroup")) >> [] {
            reply(group::get("local", "foobar"));
        }
    ).until(gref(spawned_reflectors));
    await_all_others_done();
    // wait for server
    receive (
        on(atom("Spawn5Done")) >> [] { }
    );
}

int client_part(const vector<string_pair>& args) {
    CPPA_TEST(test__remote_actor_client_part);
    auto i = find_if(args.begin(), args.end(),
                          [](const string_pair& p) { return p.first == "port"; });
    if (i == args.end()) {
        throw runtime_error("no port specified");
    }
    auto port = static_cast<uint16_t>(stoi(i->second));
    auto server = remote_actor("localhost", port);
    // remote_actor is supposed to return the same server when connecting to
    // the same host again
    for (int i = 0; i < 5; ++i) {
        auto server2 = remote_actor("localhost", port);
        CPPA_CHECK(server == server2);
    }
    send(server, atom("SpawnPing"));
    receive (
        on(atom("PingPtr"), arg_match) >> [](actor_ptr ping_actor) {
            spawn<detached>(pong, ping_actor);
        }
    );
    await_all_others_done();
    receive_response (sync_send(server, atom("SyncMsg"))) (
        others() >> [&] {
            if (self->last_dequeued() != make_cow_tuple(atom("SyncReply"))) {
                ostringstream oss;
                oss << "unexpected message; "
                    << __FILE__ << " line " << __LINE__ << ": "
                    << to_string(self->last_dequeued()) << endl;
                send(server, atom("Failure"), oss.str());
            }
            else {
                send(server, atom("Done"));
            }
        },
        after(chrono::seconds(5)) >> [&] {
            CPPA_PRINTERR("sync_send timed out!");
            send(server, atom("Timeout"));
        }
    );
    receive (
        others() >> [&] {
            CPPA_ERROR("unexpected message; "
                       << __FILE__ << " line " << __LINE__ << ": "
                       << to_string(self->last_dequeued()));
        },
        after(chrono::seconds(0)) >> [&] { }
    );
    // test 100 sync_messages
    for (int i = 0; i < 100; ++i) {
        receive_response (sync_send(server, atom("foo"), atom("bar"), i)) (
            on(atom("foo"), atom("bar"), i) >> [] {

            },
            others() >> [&] {
                CPPA_ERROR("unexpected message; "
                           << __FILE__ << " line " << __LINE__ << ": "
                           << to_string(self->last_dequeued()));
            },
            after(chrono::seconds(10)) >> [&] {
                CPPA_ERROR("unexpected timeout!");
            }
        );
    }
    spawn5_server(server, false);
    spawn5_client();
    // wait for locally spawned reflectors
    await_all_others_done();

    receive (
        on(atom("fwd"), arg_match) >> [&](const actor_ptr& fwd, const string&) {
            forward_to(fwd);
        }
    );
    // shutdown handshake
    send(server, atom("farewell"));
    receive(on(atom("cu")) >> [] { });
    shutdown();
    return CPPA_TEST_RESULT();
}

} // namespace <anonymous>

void verbose_terminate() {
    try { if (std::uncaught_exception()) throw; }
    catch (std::exception& e) {
        CPPA_PRINTERR("terminate called after throwing "
                      << to_verbose_string(e));
    }
    catch (...) {
        CPPA_PRINTERR("terminate called after throwing an unknown exception");
    }

    abort();
}

int main(int argc, char** argv) {
    set_terminate(verbose_terminate);
    announce<actor_vector>();
    cout.unsetf(ios_base::unitbuf);
    string app_path = argv[0];
    bool run_remote_actor = true;
    if (argc > 1) {
        if (strcmp(argv[1], "run_remote_actor=false") == 0) {
            run_remote_actor = false;
        }
        else {
            auto args = get_kv_pairs(argc, argv);
            return client_part(args);
        }
    }
    CPPA_TEST(test__remote_actor);
    CPPA_LOGF_TRACE("");
    //auto ping_actor = spawn(ping, 10);
    uint16_t port = 4242;
    bool success = false;
    do {
        try {
            publish(self, port, "127.0.0.1");
            success = true;
            CPPA_LOGF_DEBUG("running on port " << port);
        }
        catch (bind_failure&) {
            // try next port
            ++port;
        }
    }
    while (!success);
    thread child;
    ostringstream oss;
    if (run_remote_actor) {
        oss << app_path << " run=remote_actor port=" << port;// << " &>client.txt";
        // execute client_part() in a separate process,
        // connected via localhost socket
        child = thread([&oss]() {
            CPPA_LOGF_TRACE("lambda expression calling system()");
            string cmdstr = oss.str();
            if (system(cmdstr.c_str()) != 0) {
                CPPA_PRINTERR("FATAL: command \"" << cmdstr << "\" failed!");
                abort();
            }
        });
    }
    else {
        CPPA_PRINT("actor published at port " << port);
    }
    //CPPA_PRINT("await SpawnPing message");
    actor_ptr remote_client;
    CPPA_LOGF_DEBUG("send 'SpawnPing', expect 'PingPtr'");
    receive (
        on(atom("SpawnPing")) >> [&]() {
            remote_client = self->last_sender();
            CPPA_LOGF_ERROR_IF(!remote_client, "last_sender() == nullptr");
            CPPA_LOGF_DEBUG("spawn 10 event-based ping actors");
            reply(atom("PingPtr"), spawn_event_based_ping(10));
        }
    );
    CPPA_LOGF_DEBUG("wait until spawned ping actors are done");
    await_all_others_done();
    CPPA_CHECK_EQUAL(pongs(), 10);
    CPPA_PRINT("test remote sync_send");
    receive (
        on(atom("SyncMsg")) >> [] {
            reply(atom("SyncReply"));
        }
    );
    receive (
        on(atom("Done")) >> [] {
            // everything's fine
        },
        on(atom("Failure"), arg_match) >> [&](const string& str) {
            CPPA_ERROR(str);
        },
        on(atom("Timeout")) >> [&] {
            CPPA_ERROR("sync_send timed out");
        }
    );
    // test 100 sync messages
    CPPA_PRINT("test 100 synchronous messages");
    int i = 0;
    receive_for(i, 100) (
        others() >> [] {
            reply_tuple(self->last_dequeued());
        }
    );
    CPPA_PRINT("test group communication via network");
    // group test
    spawn5_client();
    CPPA_PRINT("test group communication via network (inverted setup)");
    spawn5_server(remote_client, true);

    self->on_sync_failure(CPPA_UNEXPECTED_MSG_CB());

    // test forward_to "over network and back"
    CPPA_PRINT("test forwarding over network 'and back'");
    auto ra = spawn<replier>();
    timed_sync_send(remote_client, chrono::seconds(5), atom("fwd"), ra, "hello replier!").await(
        [&](int forty_two) {
            CPPA_CHECK_EQUAL(forty_two, 42);
            auto from = self->last_sender();
            CPPA_CHECK_EQUAL(from, ra);
            if (from) CPPA_CHECK_EQUAL(from->is_proxy(), false);
        }
    );

    CPPA_PRINT("wait for a last goodbye");
    receive(on(atom("farewell")) >> [&] {
        send(remote_client, atom("cu"));
        CPPA_CHECKPOINT();
    });
    // wait until separate process (in sep. thread) finished execution
    if (run_remote_actor) child.join();
    shutdown();
    return CPPA_TEST_RESULT();
}
