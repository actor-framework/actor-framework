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

#include "cppa/detail/raw_access.hpp"

using namespace std;
using namespace cppa;

namespace {

typedef std::pair<std::string, std::string> string_pair;

typedef vector<actor> actor_vector;

void reflector(event_based_actor* self) {
    self->become (
        others() >> [=] {
            CPPA_PRINT("reflect and quit");
            self->quit();
            return self->last_dequeued();
        }
    );
}

void spawn5_server_impl(event_based_actor* self, actor client, group grp) {
    CPPA_LOGF_TRACE(CPPA_TARG(client, to_string)
                    << ", " << CPPA_TARG(grp, to_string));
    CPPA_CHECK(grp != invalid_group);
    self->spawn_in_group(grp, reflector);
    self->spawn_in_group(grp, reflector);
    CPPA_PRINT("send {'Spawn5'} and await {'ok', actor_vector}");
    self->sync_send(client, atom("Spawn5"), grp).then(
        on(atom("ok"), arg_match) >> [=](const actor_vector& vec) {
            CPPA_PRINT("received vector with " << vec.size() << " elements");
            self->send(grp, "Hello reflectors!", 5.0);
            if (vec.size() != 5) {
                CPPA_PRINTERR("remote client did not spawn five reflectors!");
            }
            for (auto& a : vec) {
                CPPA_PRINT("monitor actor: " << to_string(a));
                self->monitor(a);
            }
        },
        others() >> [=] {
            CPPA_UNEXPECTED_MSG(self);
            self->quit(exit_reason::unhandled_exception);
        },
        after(chrono::seconds(10)) >> [=] {
            CPPA_UNEXPECTED_TOUT();
            self->quit(exit_reason::unhandled_exception);
        }
    )
    .continue_with([=] {
        CPPA_PRINT("wait for reflected messages");
        // receive seven reply messages (2 local, 5 remote)
        auto replies = std::make_shared<int>(0);
        self->become (
            on("Hello reflectors!", 5.0) >> [=] {
                if (++*replies == 7) {
                    CPPA_PRINT("wait for DOWN messages");
                    auto downs = std::make_shared<int>(0);
                    self->become (
                        on_arg_match >> [=](const down_msg& dm) {
                            if (dm.reason != exit_reason::normal) {
                                CPPA_PRINTERR("reflector exited for non-normal exit reason!");
                            }
                            if (++*downs == 5) {
                                CPPA_CHECKPOINT();
                                self->send(client, atom("Spawn5Done"));
                                self->quit();
                            }
                        },
                        others() >> [=] {
                            CPPA_UNEXPECTED_MSG(self);
                            //self->quit(exit_reason::unhandled_exception);
                        },
                        after(chrono::seconds(2)) >> [=] {
                            CPPA_UNEXPECTED_TOUT();
                            CPPA_LOGF_ERROR("did only receive " << *downs
                                            << " down messages");
                            //self->quit(exit_reason::unhandled_exception);
                        }
                    );
                }
            },
            after(std::chrono::seconds(2)) >> [=] {
                CPPA_UNEXPECTED_TOUT();
                CPPA_LOGF_ERROR("did only receive " << *replies
                                << " responses to 'Hello reflectors!'");
                //self->quit(exit_reason::unhandled_exception);
            }
        );
    });
}

// receive seven reply messages (2 local, 5 remote)
void spawn5_server(event_based_actor* self, actor client, bool inverted) {
    if (!inverted) spawn5_server_impl(self, client, group::get("local", "foobar"));
    else {
        CPPA_PRINT("request group");
        self->sync_send(client, atom("GetGroup")).then (
            [=](const group& remote_group) {
                spawn5_server_impl(self, client, remote_group);
            }
        );
    }
}

void spawn5_client(event_based_actor* self) {
    self->become (
        on(atom("GetGroup")) >> []() -> group {
            CPPA_PRINT("received {'GetGroup'}");
            return group::get("local", "foobar");
        },
        on(atom("Spawn5"), arg_match) >> [=](const group& grp) -> any_tuple {
            CPPA_PRINT("received {'Spawn5'}");
            actor_vector vec;
            for (int i = 0; i < 5; ++i) {
                vec.push_back(spawn_in_group(grp, reflector));
            }
            return make_any_tuple(atom("ok"), std::move(vec));
        },
        on(atom("Spawn5Done")) >> [=] {
            CPPA_PRINT("received {'Spawn5Done'}");
            self->quit();
        }
    );
}

template<typename F>
void await_down(event_based_actor* self, actor ptr, F continuation) {
    self->become (
        on_arg_match >> [=](const down_msg& dm) -> bool {
            if (dm.source == ptr) {
                continuation();
                return true;
            }
            return false; // not the 'DOWN' message we are waiting for
        }
    );
}

static constexpr size_t num_pings = 10;

class client : public event_based_actor {

 public:

    client(actor server) : m_server(std::move(server)) { }

    behavior make_behavior() override {
        return spawn_ping();
    }

 private:

    behavior spawn_ping() {
        CPPA_PRINT("send {'SpawnPing'}");
        send(m_server, atom("SpawnPing"));
        return (
            on(atom("PingPtr"), arg_match) >> [=](const actor& ping) {
                auto pptr = spawn<monitored+detached+blocking_api>(pong, ping);
                await_down(this, pptr, [=] {
                    send_sync_msg();
                });
            }

        );
    }

    void send_sync_msg() {
        CPPA_PRINT("sync send {'SyncMsg', 4.2fSyncMsg}");
        sync_send(m_server, atom("SyncMsg"), 4.2f).then(
            on(atom("SyncReply")) >> [=] {
                send_foobars();
            }
        );
    }

    void send_foobars(int i = 0) {
        if (i == 0) { CPPA_PRINT("send foobars"); }
        if (i == 100) test_group_comm();
        else {
            CPPA_LOG_DEBUG("send message nr. " << (i+1));
            sync_send(m_server, atom("foo"), atom("bar"), i).then (
                on(atom("foo"), atom("bar"), i) >> [=] {
                    send_foobars(i+1);
                }
            );
        }
    }

    void test_group_comm() {
        CPPA_PRINT("test group communication via network");
        sync_send(m_server, atom("GClient")).then(
            on(atom("GClient"), arg_match) >> [=](actor gclient) {
                CPPA_CHECKPOINT();
                auto s5a = spawn<monitored>(spawn5_server, gclient, false);
                await_down(this, s5a, [=]{
                    test_group_comm_inverted();
                });
            }
        );
    }

    void test_group_comm_inverted() {
        CPPA_PRINT("test group communication via network (inverted setup)");
        become (
            on(atom("GClient")) >> [=]() -> any_tuple {
                CPPA_CHECKPOINT();
                auto cptr = last_sender();
                auto s5c = spawn<monitored>(spawn5_client);
                // set next behavior
                await_down(this, s5c, [=] {
                    CPPA_CHECKPOINT();
                    quit();
                });
                return make_any_tuple(atom("GClient"), s5c);
            }
        );
    }

    actor m_server;

};

class server : public event_based_actor {

 public:

    behavior make_behavior() override {
        return await_spawn_ping();
    }

 private:

    behavior await_spawn_ping() {
        CPPA_PRINT("await {'SpawnPing'}");
        return (
            on(atom("SpawnPing")) >> [=]() -> any_tuple {
                CPPA_PRINT("received {'SpawnPing'}");
                auto client = last_sender();
                if (!client) {
                    CPPA_PRINT("last_sender() invalid!");
                }
                CPPA_PRINT("spawn event-based ping actor");
                auto pptr = spawn<monitored>(event_based_ping, num_pings);
                CPPA_PRINT("wait until spawned ping actor is done");
                await_down(this, pptr, [=] {
                    CPPA_CHECK_EQUAL(pongs(), num_pings);
                    await_sync_msg();
                });
                return make_any_tuple(atom("PingPtr"), pptr);
            }
        );
    }

    void await_sync_msg() {
        CPPA_PRINT("await {'SyncMsg'}");
        become (
            on(atom("SyncMsg"), arg_match) >> [=](float f) -> atom_value {
                CPPA_PRINT("received {'SyncMsg', " << f << "}");
                CPPA_CHECK_EQUAL(f, 4.2f);
                await_foobars();
                return atom("SyncReply");
            }
        );
    }

    void await_foobars() {
        CPPA_PRINT("await foobars");
        auto foobars = make_shared<int>(0);
        become (
            on(atom("foo"), atom("bar"), arg_match) >> [=](int i) -> any_tuple {
                ++*foobars;
                if (i == 99) {
                    CPPA_CHECK_EQUAL(*foobars, 100);
                    test_group_comm();
                }
                return last_dequeued();
            }
        );
    }

    void test_group_comm() {
        CPPA_PRINT("test group communication via network");
        become (
            on(atom("GClient")) >> [=]() -> any_tuple {
                CPPA_CHECKPOINT();
                auto cptr = last_sender();
                auto s5c = spawn<monitored>(spawn5_client);
                await_down(this, s5c, [=] {
                    CPPA_CHECKPOINT();
                    test_group_comm_inverted(detail::raw_access::unsafe_cast(cptr));
                });
                return make_any_tuple(atom("GClient"), s5c);
            }
        );
    }

    void test_group_comm_inverted(actor cptr) {
        CPPA_PRINT("test group communication via network (inverted setup)");
        sync_send(cptr, atom("GClient")).then (
            on(atom("GClient"), arg_match) >> [=](actor gclient) {
                await_down(this, spawn<monitored>(spawn5_server, gclient, true), [=] {
                    CPPA_CHECKPOINT();
                    quit();
                });
            }
        );
    }

};

} // namespace <anonymous>

int main(int argc, char** argv) {
    announce<actor_vector>();
    announce_tuple<atom_value, int>();
    announce_tuple<atom_value, atom_value, int>();
    string app_path = argv[0];
    bool run_remote_actor = true;
    bool run_as_server = false;
    if (argc > 1) {
        if (strcmp(argv[1], "run_remote_actor=false") == 0) {
            CPPA_PRINT("don't run remote actor");
            run_remote_actor = false;
        }
        else if (strcmp(argv[1], "run_as_server") == 0) {
            CPPA_PRINT("don't run remote actor");
            run_remote_actor = false;
            run_as_server = true;
        }
        else {
            run_client_part(get_kv_pairs(argc, argv), [](uint16_t port) {
                scoped_actor self;
                auto serv = remote_actor("localhost", port);
                // remote_actor is supposed to return the same server
                // when connecting to the same host again
                {
                    auto server2 = remote_actor("localhost", port);
                    CPPA_CHECK(serv == server2);
                    std::string localhost("127.0.0.1");
                    auto server3 = remote_actor(localhost, port);
                    CPPA_CHECK(serv == server3);
                }
                auto c = self->spawn<client, monitored>(serv);
                self->receive (
                    on_arg_match >> [&](const down_msg& dm) {
                        CPPA_CHECK_EQUAL(dm.source, c);
                        CPPA_CHECK_EQUAL(dm.reason, exit_reason::normal);
                    }
                );
            });
            return CPPA_TEST_RESULT();
        }
    }
    { // lifetime scope of self
        scoped_actor self;
        auto serv = self->spawn<server, monitored>();
        uint16_t port = 4242;
        bool success = false;
        do {
            try {
                publish(serv, port, "127.0.0.1");
                success = true;
                CPPA_LOGF_DEBUG("running on port " << port);
            }
            catch (bind_failure&) {
                // try next port
                ++port;
            }
        }
        while (!success);
        do {
            CPPA_TEST(test_remote_actor);
            thread child;
            ostringstream oss;
            if (run_remote_actor) {
                oss << app_path << " run=remote_actor port=" << port
                    << to_dev_null;
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
            CPPA_CHECKPOINT();
            self->receive (
                on_arg_match >> [&](const down_msg& dm) {
                    CPPA_CHECK_EQUAL(dm.source, serv);
                    CPPA_CHECK_EQUAL(dm.reason, exit_reason::normal);
                }
            );
            // wait until separate process (in sep. thread) finished execution
            CPPA_CHECKPOINT();
            if (run_remote_actor) child.join();
            CPPA_CHECKPOINT();
            self->await_all_other_actors_done();
        }
        while (run_as_server);
    } // lifetime scope of self
    await_all_actors_done();
    shutdown();
    return CPPA_TEST_RESULT();
}
