#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>
#include <functional>
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;

static const char* usage =
"Usage: distributed_math_actor_example [OPTIONS]                            \n"
"                                                                           \n"
" General options:                                                          \n"
"  -s | --server          run in server mode                                \n"
"  -c | --client          run in client mode                                \n"
"  -p PORT | --port=PORT  publish at PORT (server mode)                     \n"
"                         connect to PORT (client mode)                     \n"
"                                                                           \n"
" Client options:                                                           \n"
"                                                                           \n"
"  -h HOST | --host=HOST  connect to HOST, default: localhost (client mode) \n";

struct math_actor : event_based_actor {
    void init() {
        // execute this behavior until actor terminates
        become (
            on(atom("plus"), arg_match) >> [](int a, int b) {
                reply(atom("result"), a + b);
            },
            on(atom("minus"), arg_match) >> [](int a, int b) {
                reply(atom("result"), a - b);
            },
            on(atom("quit")) >> [=]() {
                quit();
            }
        );
    }
};

option<atom_value> get_op(const string& from) {
    if (from.size() == 1) {
        switch (from[0]) {
            case '+': return atom("plus");
            case '-': return atom("minus");
            default: break;
        }
    }
    return {};
}

option<int> toint(const string& str) {
    char* endptr = nullptr;
    int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
    if (endptr != nullptr && *endptr == '\0') {
        return result;
    }
    return {};
}

inline std::vector<std::string> split(const std::string& str, char delim) {
    vector<string> result;
    stringstream strs{str};
    string tmp;
    while (std::getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

void client_repl(actor_ptr server, string host, uint16_t port) {
    self->monitor(server);
    string line;
    while (getline(cin, line)) {
        match (split(line, ' ')) (
            on(toint, get_op, toint) >> [&](int x, atom_value op, int y) {
                // send request
                send(server, op, x, y);
                // wait for result
                bool done = false;
                receive_while (gref(done) == false) (
                    on(atom("result"), arg_match) >> [&](int result) {
                        cout << x << " "
                             << to_string(op) << " "
                             << y << " = "
                             << result
                             << endl;
                        done = true;
                    },
                    on(atom("DOWN"), arg_match) >> [&](uint32_t reason) {
                        cerr << "*** server exited with reason = " << reason
                             << endl;
                        send(self, atom("reconnect"));
                    },
                    on(atom("reconnect")) >> [&] {
                        cout << "try reconnecting ... " << flush;
                        try {
                            server = remote_actor(host, port);
                            self->monitor(server);
                            send(server, op, x, y);
                            cout << "success" << endl;
                        }
                        catch (exception&) {
                            cout << "failed, try again in 3s" << endl;
                            delayed_send(self, chrono::seconds(3), atom("reconnect"));
                        }
                    },
                    others() >> [&] {
                        cerr << "unexpected: "
                             << to_string(self->last_dequeued())
                             << endl;
                    }
                );
            },
            others() >> [&] {
                cerr << "*** invalid format, please use: X +/- Y" << endl;
            }
        );
    }
}

function<option<string> (const string&)> kvp(const string& key) {
    auto prefix = "--" + key;
    prefix += "=";
    return [prefix](const string& input) -> option<string> {
        if (   input.compare(0, prefix.size(), prefix) == 0
               // accept '-key=' as well
            || input.compare(1, prefix.size(), prefix) == 0) {
            return input.substr(prefix.size());
        }
        return {};
    };
}

auto on_opt(char short_opt, const char* long_opt) -> decltype(on("", val<string>) || on(kvp(""))) {
    const char lhs[] = {'-', short_opt, '\0' };
    const char* lhs_str = lhs;
    return on(lhs_str, val<string>) || on(kvp(long_opt));
}

int main(int argc, char** argv) {
    string mode;
    uint16_t port = 0;
    string host;
    vector<string> args(argv + 1, argv + argc);
    auto set_mode = [&](string arg) -> bool {
        if (!mode.empty()) {
            cerr << "mode already set to " << mode << endl;
            return false;
        }
        mode = move(arg);
        return true;
    };
    bool args_valid = !args.empty() && match_stream<string>(begin(args), end(args)) (
        on_opt('p', "port") >> [&](const string& arg) -> bool {
            auto p = toint(arg);
            if (p && *p > 1024 && *p < 0xFFFFF) {
                port = static_cast<uint16_t>(*p);
                return true;
            }
            cerr << port << " is not a valid port" << endl;
            return false; // no match
        },
        on_opt('h', "host") >> [&](const string& arg) -> bool {
            if (host.empty()) {
                host = arg;
                return true;
            }
            cerr << "host previously set to \"" << arg << "\"" << endl;
            return false;
        },
        (on("-s") || on("--server")) >> [&]() -> bool {
            return set_mode("server");
        },
        (on("-c") || on("--client")) >> [&]() -> bool {
            return set_mode("client");
        }
    );
    if (!args_valid || port == 0 || mode.empty() || (mode == "server" && !host.empty())) {
        if (port == 0) {
            cerr << "no port given" << endl;
        }
        if (mode.empty()) {
            cerr << "no mode given" << endl;
        }
        if (mode == "server" && !host.empty()) {
            cerr << "host is a client-only option" << endl;
        }
        cout << endl << usage << endl;
        return -1;
    }
    if (mode == "server") {
        try {
            publish(spawn<math_actor>(), port);
        }
        catch (bind_failure& bf) {
            cerr << "unable to publish actor at port "
                 << port << ":" << bf.what() << endl;
        }
    }
    else {
        if (host.empty()) {
            host = "localhost";
        }
        try {
            auto server = remote_actor(host, port);
            client_repl(server, host, port);
        }
        catch (exception& e) {
            cerr << "unable to connect to remote actor at host \""
                 << host << "\" on port " << port << endl;
        }
    }
    await_all_others_done();
    shutdown();
    return 0;
}

