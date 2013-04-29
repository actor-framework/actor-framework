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

void reflector() {
    become (
        others() >> [=] {
            CPPA_LOGF_INFO("reflect and quit");
            reply_tuple(self->last_dequeued());
            self->quit();
        }
    );
}

void replier() {
    become (
        others() >> [=] {
            CPPA_LOGF_INFO("reply and quit");
            reply(42);
            self->quit();
        }
    );
}

void spawn5_server_impl(actor_ptr client, group_ptr grp) {
    CPPA_LOGF_TRACE(CPPA_TARG(client, to_string)
                    << ", " << CPPA_TARG(grp, to_string));
    spawn_in_group(grp, reflector);
    spawn_in_group(grp, reflector);
    CPPA_LOGF_INFO("send {'Spawn5'} and await {'ok', actor_vector}");
    sync_send(client, atom("Spawn5"), grp).then(
        on(atom("ok"), arg_match) >> [&](const actor_vector& vec) {
            CPPA_LOGF_INFO("received vector with " << vec.size() << " elements");
            send(grp, "Hello reflectors!", 5.0);
            if (vec.size() != 5) {
                CPPA_PRINTERR("remote client did not spawn five reflectors!");
            }
            for (auto& a : vec) self->monitor(a);
        },
        others() >> [] {
            CPPA_UNEXPECTED_MSG();
            self->quit(exit_reason::unhandled_exception);
        },
        after(chrono::seconds(10)) >> [] {
            CPPA_UNEXPECTED_TOUT();
            self->quit(exit_reason::unhandled_exception);
        }
    )
    .continue_with([=] {
        CPPA_PRINT("wait for reflected messages");
        // receive seven reply messages (2 local, 5 remote)
        auto replies = std::make_shared<int>(0);
        become (
            on("Hello reflectors!", 5.0) >> [=] {
                if (++*replies == 7) {
                    CPPA_PRINT("wait for DOWN messages");
                    auto downs = std::make_shared<int>(0);
                    become (
                        on(atom("DOWN"), arg_match) >> [=](std::uint32_t reason) {
                            if (reason != exit_reason::normal) {
                                CPPA_PRINTERR("reflector exited for non-normal exit reason!");
                            }
                            if (++*downs == 5) {
                                CPPA_CHECKPOINT();
                                send(client, atom("Spawn5Done"));
                                self->quit();
                            }
                        },
                        others() >> [] {
                            CPPA_UNEXPECTED_MSG();
                            self->quit(exit_reason::unhandled_exception);
                        },
                        after(chrono::seconds(2)) >> [] {
                            CPPA_UNEXPECTED_TOUT();
                            self->quit(exit_reason::unhandled_exception);
                        }
                    );
                }
            },
            after(std::chrono::seconds(2)) >> [] {
                CPPA_UNEXPECTED_TOUT();
                self->quit(exit_reason::unhandled_exception);
            }
        );
    });
}

// receive seven reply messages (2 local, 5 remote)
void spawn5_server(actor_ptr client, bool inverted) {
    if (!inverted) spawn5_server_impl(client, group::get("local", "foobar"));
    else {
        CPPA_LOGF_INFO("request group");
        sync_send(client, atom("GetGroup")).then (
            [&](const group_ptr& remote_group) {
                spawn5_server_impl(client, remote_group);
            }
        );
    }
}

void spawn5_client() {
    become (
        on(atom("GetGroup")) >> [] {
            CPPA_LOGF_INFO("received {'GetGroup'}");
            reply(group::get("local", "foobar"));
        },
        on(atom("Spawn5"), arg_match) >> [&](const group_ptr& grp) {
            CPPA_LOGF_INFO("received {'Spawn5'}");
            actor_vector vec;
            for (int i = 0; i < 5; ++i) {
                vec.push_back(spawn_in_group(grp, reflector));
            }
            reply(atom("ok"), std::move(vec));
        },
        on(atom("Spawn5Done")) >> [] {
            CPPA_LOGF_INFO("received {'Spawn5Done'}");
            self->quit();
        }
    );
}

int client_part(const vector<string_pair>& args) {
    CPPA_TEST(test_remote_actor_client_part);
    auto i = find_if(args.begin(), args.end(),
                          [](const string_pair& p) { return p.first == "port"; });
    if (i == args.end()) {
        throw runtime_error("no port specified");
    }
    auto port = static_cast<uint16_t>(stoi(i->second));
    auto server = remote_actor("localhost", port);
    // remote_actor is supposed to return the same server when connecting to
    // the same host again
    {
        auto server2 = remote_actor("localhost", port);
        CPPA_CHECK(server == server2);
    }
    send(server, atom("SpawnPing"));
    receive (
        on(atom("PingPtr"), arg_match) >> [](actor_ptr ping_actor) {
            spawn<detached + blocking_api>(pong, ping_actor);
        }
    );
    await_all_others_done();
    sync_send(server, atom("SyncMsg")).await(
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
            CPPA_FAILURE("unexpected message; "
                         << __FILE__ << " line " << __LINE__ << ": "
                         << to_string(self->last_dequeued()));
        },
        after(chrono::seconds(0)) >> [&] { }
    );
    // test 100 sync_messages
    for (int i = 0; i < 100; ++i) {
        sync_send(server, atom("foo"), atom("bar"), i).await(
            on(atom("foo"), atom("bar"), i) >> [] { },
            others() >> [&] {
                CPPA_FAILURE("unexpected message; "
                             << __FILE__ << " line " << __LINE__ << ": "
                             << to_string(self->last_dequeued()));
            },
            after(chrono::seconds(10)) >> [&] {
                CPPA_FAILURE("unexpected timeout!");
            }
        );
    }
    CPPA_CHECKPOINT();
    spawn5_server(server, false);
    self->exec_behavior_stack();
    await_all_others_done();
    CPPA_CHECKPOINT();
    spawn5_client();
    self->exec_behavior_stack();
    await_all_others_done();
    CPPA_CHECKPOINT();
    // wait for locally spawned reflectors
    await_all_others_done();
    CPPA_CHECKPOINT();

    receive (
        on(atom("fwd"), arg_match) >> [&](const actor_ptr& fwd, const string&) {
            forward_to(fwd);
        }
    );
    CPPA_CHECKPOINT();
    // shutdown handshake
    send(server, atom("farewell"));
    CPPA_CHECKPOINT();
    receive(on(atom("cu")) >> [] { });
    CPPA_CHECKPOINT();
    shutdown();
    CPPA_CHECKPOINT();
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
    CPPA_TEST(test_remote_actor);
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
        oss << app_path << " run=remote_actor port=" << port << " &>/dev/null";
        // execute client_part() in a separate process,
        // connected via localhost socket
        child = thread([&oss]() {
            CPPA_LOGC_TRACE("NONE", "main$thread_launcher", "");
            string cmdstr = oss.str();
            if (system(cmdstr.c_str()) != 0) {
                CPPA_PRINTERR("FATAL: command \"" << cmdstr << "\" failed!");
                abort();
            }
        });
    }
    else { CPPA_PRINT("actor published at port " << port); }
    //CPPA_PRINT("await SpawnPing message");
    actor_ptr remote_client;
    CPPA_LOGF_INFO("receive 'SpawnPing', reply with 'PingPtr'");
    receive (
        on(atom("SpawnPing")) >> [&]() {
            remote_client = self->last_sender();
            CPPA_LOGF_ERROR_IF(!remote_client, "last_sender() == nullptr");
            CPPA_LOGF_INFO("spawn event-based ping actor");
            reply(atom("PingPtr"), spawn_event_based_ping(10));
        }
    );
    CPPA_LOGF_INFO("wait until spawned ping actor is done");
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
            CPPA_FAILURE(str);
        },
        on(atom("Timeout")) >> [&] {
            CPPA_FAILURE("sync_send timed out");
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
    spawn5_client();
    self->exec_behavior_stack();
    await_all_others_done();
    CPPA_PRINT("test group communication via network (inverted setup)");
    spawn5_server(remote_client, true);
    self->exec_behavior_stack();
    await_all_others_done();

    self->on_sync_failure(CPPA_UNEXPECTED_MSG_CB());

    // test forward_to "over network and back"
    CPPA_PRINT("test forwarding over network 'and back'");
    auto ra = spawn(replier);
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
