/******************************************************************************\
 * This program is a distributed version of the math_actor example.           *
 * Client and server use a stateless request/response protocol and the client *
 * is failure resilient by using a FIFO request queue.                        *
 * The client auto-reconnects and also allows for server reconfiguration.     *
 *                                                                            *
 * Run server at port 4242:                                                   *
 * - ./build/bin/distributed_math_actor -s -p 4242                            *
 *                                                                            *
 * Run client at the same host:                                               *
 * - ./build/bin/distributed_math_actor -c -p 4242                            *
\******************************************************************************/

#include <vector>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>
#include <functional>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::placeholders;

// our "service"
void calculator() {
    become (
        on(atom("plus"), arg_match) >> [](int a, int b) {
            reply(atom("result"), a + b);
        },
        on(atom("minus"), arg_match) >> [](int a, int b) {
            reply(atom("result"), a - b);
        },
        on(atom("quit")) >> [=]() {
            self->quit();
        }
    );
}

inline string trim(std::string s) {
    auto not_space = [](char c) { return !isspace(c); };
    // trim left
    s.erase(s.begin(), find_if(s.begin(), s.end(), not_space));
    // trim right
    s.erase(find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return std::move(s);
}

void client_bhvr(const string& host, uint16_t port, const actor_ptr& server) {
    // recover from sync failures by trying to reconnect to server
    if (!self->has_sync_failure_handler()) {
        self->on_sync_failure([=] {
            aout << "*** lost connection to " << host << ":" << port << endl;
            client_bhvr(host, port, nullptr);
        });
    }
    // connect to server if needed
    if (!server) {
        aout << "*** try to connect to " << host << ":" << port << endl;
        try {
            auto new_serv = remote_actor(host, port);
            self->monitor(new_serv);
            aout << "reconnection succeeded" << endl;
            client_bhvr(host, port, new_serv);
            return;
        }
        catch (exception&) {
            aout << "connection failed, try again in 3s" << endl;
            delayed_send(self, chrono::seconds(3), atom("reconnect"));
        }
    }
    become (
        on_arg_match.when(_x1.in({atom("plus"), atom("minus")}) && gval(server) != nullptr) >> [=](atom_value op, int lhs, int rhs) {
            sync_send_tuple(server, self->last_dequeued()).then(
                on(atom("result"), arg_match) >> [=](int result) {
                    aout << lhs << " "
                         << to_string(op) << " "
                         << rhs << " = "
                         << result << endl;
                }
            );
        },
        on(atom("DOWN"), arg_match) >> [=](uint32_t) {
            aout << "*** server down, try to reconnect ..." << endl;
            client_bhvr(host, port, nullptr);
        },
        on(atom("rebind"), arg_match) >> [=](const string& host, uint16_t port) {
            aout << "*** rebind to new server: " << host << ":" << port << endl;
            client_bhvr(host, port, nullptr);
        },
        on(atom("reconnect")) >> [=] {
            client_bhvr(host, port, nullptr);
        }
    );
}

void client_repl(const string& host, uint16_t port) {
    typedef cow_tuple<atom_value, int, int> request;
    // keeps track of requests and tries to reconnect on server failures
    aout << "Usage:\n"
            "quit                   Quit the program\n"
            "<x> + <y>              Calculate <x>+<y> and print result\n"
            "<x> - <y>              Calculate <x>-<y> and print result\n"
            "connect <host> <port>  Reconfigure server"
         << endl << endl;
    string line;
    auto client = spawn(client_bhvr, host, port, nullptr);
    const char connect[] = "connect ";
    while (getline(cin, line)) {
        line = trim(std::move(line)); // ignore leading and trailing whitespaces
        if (line == "quit") {
            // force client to quit
            send_exit(client, exit_reason::user_defined);
            return;
        }
        // the STL way of line.starts_with("connect")
        else if (equal(begin(connect), end(connect) - 1, begin(line))) {
            match_split(line, ' ') (
                on("connect", arg_match) >> [&](string& host, string& sport) {
                    try {
                        auto lport = std::stoul(sport);
                        if (lport < std::numeric_limits<uint16_t>::max()) {
                            auto port = static_cast<uint16_t>(lport);
                            send(client, atom("rebind"), move(host), port);
                        }
                        else {
                            aout << lport << " is not a valid port" << endl;
                        }
                    }
                    catch (std::exception& e) {
                        aout << "\"" << sport << "\" is not an unsigned integer"
                             << endl;
                    }
                },
                others() >> [] {
                    aout << "*** usage: connect <host> <port>" << endl;
                }
            );
        }
        else {
            auto toint = [](const string& str) -> option<int> {
                try { return {std::stoi(str)}; }
                catch (std::exception&) {
                    aout << "\"" << str << "\" is not an integer" << endl;
                    return {};
                }
            };

            bool success = false;
            auto first = begin(line);
            auto last = end(line);
            auto pos = find_if(first, last, [](char c) { return c == '+' || c == '-'; });
            if (pos != last) {
                auto lsub = trim(string(first, pos));
                auto rsub = trim(string(pos + 1, last));
                auto lhs = toint(lsub);
                auto rhs = toint(rsub);
                if (lhs && rhs) {
                    auto op = (*pos == '+') ? atom("plus") : atom("minus");
                    send(client, op, *lhs, *rhs);
                }
            }
            else if (!success) {
                aout << "*** invalid format; usage: <x> [+|-] <y>" << endl;
            }
        }
    }
}

int main(int argc, char** argv) {
    string mode;
    string host;
    uint16_t port = 0;
    options_description desc;
    auto set_mode = [&](const string& arg) -> function<bool()> {
        return [arg,&mode]() -> bool {
            if (!mode.empty()) {
                cerr << "mode already set to " << mode << endl;
                return false;
            }
            mode = move(arg);
            return true;
        };
    };
    string copts = "client options";
    string sopts = "server options";
    bool args_valid = match_stream<string> (argv + 1, argv + argc) (
        on_opt1('p', "port",   &desc, "set port") >> rd_arg(port),
        on_opt1('H', "host",   &desc, "set host (default: localhost)", copts) >> rd_arg(host),
        on_opt0('s', "server", &desc, "run in server mode", sopts) >> set_mode("server"),
        on_opt0('c', "client", &desc, "run in client mode", copts) >> set_mode("client"),
        on_opt0('h', "help",   &desc, "print help") >> print_desc_and_exit(&desc)
    );
    if (!args_valid || port == 0 || mode.empty()) {
        if (port == 0) cerr << "*** no port specified" << endl;
        if (mode.empty()) cerr << "*** no mode specified" << endl;
        cerr << endl;
        auto description_printer = print_desc(&desc, cerr);
        description_printer();
        return -1;
    }
    if (mode == "server") {
        try {
            // try to publish math actor at given port
            publish(spawn(calculator), port);
        }
        catch (exception& e) {
            cerr << "*** unable to publish math actor at port " << port << "\n"
                 << to_verbose_string(e) // prints exception type and e.what()
                 << endl;
        }
    }
    else {
        if (host.empty()) host = "localhost";
        client_repl(host, port);
    }
    await_all_others_done();
    shutdown();
    return 0;
}
