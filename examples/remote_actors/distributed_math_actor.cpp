#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>
#include <functional>
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::placeholders;

static const char* s_usage = R"___(
Usage: distributed_math_actor_example [OPTIONS]

 General options:
  -h | --help              Print this text and quit
  -v | --version           Print the program version and quit
  -s | --server            Run in server mode
  -c | --client            Run in client mode
  -p <arg> | --port=<arg>  Publish actor at port <arg> (server)
                           Connect to remote actor at port <arg> (client)
 Client options:
  -t <arg> | --host=<arg>  Connect to host <arg>, default: localhost
)___";

static const char* s_interactive_usage = R"___(
quit           Quit the program
<x> + <y>      Calculate <x>+<y> and print result
<x> - <y>      Calculate <x>-<y> and print result
)___";


static const char* s_version =
"libcppa distributed math actor example version 1.0";

actor_ptr s_printer;

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

option<int> toint(const string& str) {
    if (str.empty()) return {};
    char* endptr = nullptr;
    int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
    if (endptr != nullptr && *endptr == '\0') {
        return result;
    }
    return {};
}

inline string& ltrim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](char c) { return !isspace(c); }));
    return s;
}

inline string& rtrim(std::string& s) {
    s.erase(find_if(s.rbegin(), s.rend(), [](char c) { return !isspace(c); }).base(), s.end());
    return s;
}

inline string& trim(std::string& s) {
    return ltrim(rtrim(s));
}

void client_repl(actor_ptr server, string host, uint16_t port) {
    typedef cow_tuple<atom_value, int, int> request;
    // keeps track of requests and tries to reconnect on server failures
    auto client = factory::event_based([=](actor_ptr* serv, vector<request>* q) {
        self->monitor(*serv);
        auto send_next_request = [=] {
            if (!q->empty()) {
                *serv << q->front();
            }
        };
        self->become (
            on<atom_value, int, int>().when(_x1.in({atom("plus"), atom("minus")})) >> [=] {
                if (q->empty()) {
                    *serv << self->last_dequeued();
                }
                q->push_back(*tuple_cast<atom_value, int, int>(self->last_dequeued()));
            },
            on(atom("result"), arg_match) >> [=](int result) {
                if (q->empty()) {
                    send(s_printer, "received a result, "
                                    "but didn't send a request");
                    return;
                }
                ostringstream oss;
                auto& r = q->front();
                oss << get<1>(r) << " "
                    << to_string(get<0>(r)) << " "
                    << get<2>(r)
                    << " = " << result;
                send(s_printer, oss.str());
                q->erase(q->begin());
                send_next_request();
            },
            on(atom("DOWN"), arg_match) >> [=](uint32_t reason) {
                ostringstream oss;
                oss << "*** server exited with reason = " << reason;
                send(s_printer, oss.str());
                send(self, atom("reconnect"));
            },
            on(atom("reconnect")) >> [=] {
                try {
                    *serv = remote_actor(host, port);
                    self->monitor(*serv);
                    send(s_printer, "reconnection succeeded");
                    send_next_request();
                }
                catch (exception&) {
                    send(s_printer, "reconnection failed, try again in 3s");
                    delayed_send(self, chrono::seconds(3), atom("reconnect"));
                }
            },
            on(atom("quit")) >> [=] {
                self->quit();
            },
            others() >> [] {
                forward_to(s_printer);
            }
        );
    }).spawn(server);
    send(s_printer, s_interactive_usage);
    string line;
    while (getline(cin, line)) {
        trim(line);
        if (line == "quit") {
            send(client, atom("quit"));
            send(s_printer, atom("quit"));
            return;
        }
        bool success = false;
        auto pos = find_if(begin(line), end(line), [](char c) { return c == '+' || c == '-'; });
        if (pos != end(line)) {
            string lsub(begin(line), pos);
            string rsub(pos + 1, end(line));
            auto lhs = toint(trim(lsub));
            auto rhs = toint(trim(rsub));
            if (lhs && rhs) {
                auto op = (*pos == '+') ? atom("plus") : atom("minus");
                send(client, op, *lhs, *rhs);
            }
            else {
                if (!lhs) {
                    send(s_printer, "\"" + lsub + "\" is not an integer");
                }
                if (!rhs) {
                    send(s_printer, "\"" + rsub + "\" is not an integer");
                }
            }
        }
        else if (!success) {
            send(s_printer, "*** invalid format, please use: X +/- Y");
        }
    }
}

auto on_opt(char short_opt, const char* long_opt) -> decltype(on("", val<string>) || on(function<option<string> (const string&)>())) {
    const char short_flag_arr[] = {'-', short_opt, '\0' };
    const char* lhs_str = short_flag_arr;
    string prefix = "--";
    prefix += long_opt;
    prefix += "=";
    function<option<string> (const string&)> kvp = [prefix](const string& input) -> option<string> {
        if (   input.compare(0, prefix.size(), prefix) == 0
               // accept '-key=' as well
            || input.compare(1, prefix.size(), prefix) == 0) {
            return input.substr(prefix.size());
        }
        return {};
    };
    return on(lhs_str, val<string>) || on(kvp);
}

auto on_void_opt(char short_opt, const char* long_opt) -> decltype(on<string>().when(_x1.in(vector<string>()))) {
    const char short_flag_arr[] = {'-', short_opt, '\0' };
    vector<string> opt_strs = { short_flag_arr };
    opt_strs.push_back(string("-") + long_opt);
    string str = "-";
    str += opt_strs.back();
    opt_strs.push_back(std::move(str));
    return on<string>().when(_x1.in(opt_strs));
}

int main(int argc, char** argv) {
    s_printer = factory::event_based([] {
        self->become (
            on_arg_match >> [](const string& str) {
                cout << str << endl;
            },
            on(atom("quit")) >> [] {
                self->quit();
            },
            others() >> [] {
                cout << to_string(self->last_dequeued()) << endl;
            }
        );
    }).spawn();
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
        on_opt('t', "host") >> [&](const string& arg) -> bool {
            if (host.empty()) {
                host = arg;
                return true;
            }
            cerr << "host previously set to \"" << arg << "\"" << endl;
            return false;
        },
        on_void_opt('s', "server") >> [&]() -> bool {
            return set_mode("server");
        },
        on_void_opt('c', "client") >> [&]() -> bool {
            return set_mode("client");
        },
        on_void_opt('h', "help") >> [] {
            cout << s_usage << endl;
            exit(0);
        },
        on_void_opt('v', "version") >> [] {
            cout << s_version << endl;
            exit(0);
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
            cerr << "--host=<arg> is a client-only option" << endl;
        }
        cout << endl << s_usage << endl;
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
