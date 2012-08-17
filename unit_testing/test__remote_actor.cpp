#include <thread>
#include <string>
#include <cstring>
#include <sstream>
#include <iostream>

#include "test.hpp"
#include "ping_pong.hpp"
#include "cppa/cppa.hpp"
#include "cppa/exception.hpp"

using std::cout;
using std::cerr;
using std::endl;

using namespace cppa;

namespace {

std::vector<string_pair> get_kv_pairs(int argc, char** argv, int begin = 1) {
    std::vector<string_pair> result;
    for (int i = begin; i < argc; ++i) {
        auto vec = split(argv[i], '=');
        if (vec.size() != 2) {
            cerr << "\"" << argv[i] << "\" is not a key-value pair" << endl;
        }
        else if (std::any_of(result.begin(), result.end(),
                             [&](const string_pair& p) { return p.first == vec[0]; })) {
            cerr << "key \"" << vec[0] << "\" is already defined" << endl;
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

int client_part(const std::vector<string_pair>& args) {
    CPPA_TEST(test__remote_actor_client_part);
    auto i = std::find_if(args.begin(), args.end(),
                          [](const string_pair& p) { return p.first == "port"; });
    if (i == args.end()) {
        throw std::runtime_error("no port specified");
    }
    auto port = static_cast<std::uint16_t>(std::stoi(i->second));
    auto server = remote_actor("localhost", port);
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
                std::ostringstream oss;
                oss << "unexpected message; "
                    << __FILE__ << " line " << __LINE__ << ": "
                    << to_string(self->last_dequeued()) << endl;
                send(server, atom("Failure"), oss.str());
            }
            else {
                send(server, atom("Done"));
            }
        },
        after(std::chrono::seconds(5)) >> [&] {
            cerr << "sync_send timed out!" << endl;
            send(server, atom("Timeout"));
        }
    );
    receive (
        others() >> [&] {
            CPPA_ERROR("unexpected message; "
                       << __FILE__ << " line " << __LINE__ << ": "
                       << to_string(self->last_dequeued()));
        },
        after(std::chrono::seconds(0)) >> [&] { }
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
            after(std::chrono::seconds(10)) >> [&] {
                CPPA_ERROR("unexpected timeout!");
            }
        );
    }
    // test group communication
    auto grp = group::anonymous();
    spawn_in_group<reflector>(grp);
    spawn_in_group<reflector>(grp);
    receive_response (sync_send(server, atom("Spawn5"), grp)) (
        on(atom("ok")) >> [&] {
            send(grp, "Hello reflectors!", 5.0);
        },
        after(std::chrono::seconds(10)) >> [&] {
            CPPA_ERROR("unexpected timeout!");
        }
    );
    // receive seven reply messages (2 local, 5 remote)
    int x = 0;
    receive_for(x, 7) (
        on("Hello reflectors!", 5.0) >> [] { },
        others() >> [&] {
            CPPA_ERROR("unexpected message; "
                       << __FILE__ << " line " << __LINE__ << ": "
                       << to_string(self->last_dequeued()));
        }
    );
    // wait for locally spawned reflectors
    await_all_others_done();
    send(server, atom("farewell"));
    shutdown();
    return CPPA_TEST_RESULT;
}

} // namespace <anonymous>

int main(int argc, char** argv) {
    std::string app_path = argv[0];
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
    //auto ping_actor = spawn(ping, 10);
    std::uint16_t port = 4242;
    bool success = false;
    do {
        try {
            publish(self, port);
            success = true;
        }
        catch (bind_failure&) {
            // try next port
            ++port;
        }
    }
    while (!success);
    std::thread child;
    std::ostringstream oss;
    if (run_remote_actor) {
        oss << app_path << " run=remote_actor port=" << port;// << " &>client.txt";
        // execute client_part() in a separate process,
        // connected via localhost socket
        child = std::thread([&oss]() {
            std::string cmdstr = oss.str();
            if (system(cmdstr.c_str()) != 0) {
                cerr << "FATAL: command \"" << cmdstr << "\" failed!" << endl;
                abort();
            }
        });
    }
    else {
        cout << "actor published at port " << port << endl;
    }
    //cout << "await SpawnPing message" << endl;
    receive (
        on(atom("SpawnPing")) >> []() {
            reply(atom("PingPtr"), spawn_event_based_ping(10));
        }
    );
    await_all_others_done();
    CPPA_CHECK_EQUAL(10, pongs());
    cout << "test remote sync_send" << endl;
    receive (
        on(atom("SyncMsg")) >> [] {
            reply(atom("SyncReply"));
        }
    );
    receive (
        on(atom("Done")) >> [] {
            // everything's fine
        },
        on(atom("Failure"), arg_match) >> [&](const std::string& str) {
            CPPA_ERROR(str);
        },
        on(atom("Timeout")) >> [&] {
            CPPA_ERROR("sync_send timed out");
        }
    );
    // test 100 sync messages
    cout << "test 100 synchronous messages" << endl;
    int i = 0;
    receive_for(i, 100) (
        others() >> [] {
            reply_tuple(self->last_dequeued());
        }
    );
    cout << "test group communication via network" << endl;
    // group test
    receive (
        on(atom("Spawn5"), arg_match) >> [](const group_ptr& grp) {
            for (int i = 0; i < 5; ++i) {
                spawn_in_group<reflector>(grp);
            }
            reply(atom("ok"));
        }
    );
    await_all_others_done();
    cout << "wait for a last goodbye" << endl;
    receive (
        on(atom("farewell")) >> [] { }
    );
    // wait until separate process (in sep. thread) finished execution
    if (run_remote_actor) child.join();
    shutdown();
    return CPPA_TEST_RESULT;
}
