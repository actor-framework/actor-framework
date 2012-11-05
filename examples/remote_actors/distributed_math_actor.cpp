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

// our service provider
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

namespace {

// this actor manages the output of the program and has a per-actor cache
actor_ptr s_printer = factory::event_based([](map<actor_ptr,string>* out) {
    auto flush_output = [out](const actor_ptr& s) {
        auto i = out->find(s);
        if (i != out->end()) {
            auto& ref = i->second;
            if (!ref.empty()) {
                cout << move(ref) << flush;
                ref = string();
            }
        }
    };
    self->become (
        on(atom("add"), arg_match) >> [=](string& str) {
            if (!str.empty()) {
                auto s = self->last_sender();
                auto i = out->find(s);
                if (i == out->end()) {
                    i = out->insert(make_pair(s, move(str))).first;
                    // monitor actor to flush its output on exit
                    self->monitor(s);
                }
                auto& ref = i->second;
                ref += move(str);
                if (ref.back() == '\n') {
                    cout << move(ref) << flush;
                    ref = string();
                }
            }
        },
        on(atom("flush")) >> [=] {
            flush_output(self->last_sender());
        },
        on(atom("DOWN"), any_vals) >> [=] {
            auto s = self->last_sender();
            flush_output(s);
            out->erase(s);
        },
        on(atom("quit")) >> [] {
            self->quit();
        },
        others() >> [] {
            cout << "*** unexpected: "
                 << to_string(self->last_dequeued())
                 << endl;
        }
    );
}).spawn();

} // namespace <anonymous>

// an output stream sending messages to s_printer
struct actor_ostream {

    typedef actor_ostream& (*fun_type)(actor_ostream&);

    virtual ~actor_ostream() { }

    virtual actor_ostream& write(string arg) {
        send(s_printer, atom("add"), move(arg));
        return *this;
    }

    virtual actor_ostream& flush() {
        send(s_printer, atom("flush"));
        return *this;
    }

};

inline actor_ostream& operator<<(actor_ostream& o, string arg) {
    return o.write(move(arg));
}

template<typename T>
inline typename enable_if<is_convertible<T,string>::value == false, actor_ostream&>::type
operator<<(actor_ostream& o, T&& arg) {
    return o.write(to_string(std::forward<T>(arg)));
}

inline actor_ostream& operator<<(actor_ostream& o, actor_ostream::fun_type f) {
    return f(o);
}

namespace { actor_ostream aout; }

inline actor_ostream& endl(actor_ostream& o) { return o.write("\n"); }
inline actor_ostream& flush(actor_ostream& o) { return o.flush(); }

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

option<int> toint(const string& str) {
    if (str.empty()) return {};
    char* endptr = nullptr;
    int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
    if (endptr != nullptr && *endptr == '\0') {
        return result;
    }
    return {};
}

template<typename T>
struct project_helper;

template<>
struct project_helper<string> {
    template<typename T>
    inline option<T> convert(const string& from, typename enable_if<is_integral<T>::value>::type* = 0) {
        char* endptr = nullptr;
        auto result = static_cast<T>(strtol(from.c_str(), &endptr, 10));
        if (endptr != nullptr && *endptr == '\0') return result;
        return {};
    }
};

template<typename From, typename To>
option<To> projection(const From& from) {
    project_helper<From> f;
    return f.template convert<To>(from);
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
                    aout << "received a result, but didn't send a request\n";
                    return;
                }
                ostringstream oss;
                auto& r = q->front();
                aout << get<1>(r) << " "
                     << to_string(get<0>(r)) << " "
                     << get<2>(r)
                     << " = " << result
                     << endl;
                q->erase(q->begin());
                send_next_request();
            },
            on(atom("DOWN"), arg_match) >> [=](uint32_t reason) {
                if (*serv == self->last_sender()) {
                    serv->reset(); // sets *serv = nullptr
                    ostringstream oss;
                    aout << "*** server exited with reason = " << reason
                         << ", try to reconnect"
                         << endl;
                    send(self, atom("reconnect"));
                }
            },
            on(atom("reconnect")) >> [=] {
                if (*serv != nullptr) return;
                try {
                    *serv = remote_actor(host, port);
                    self->monitor(*serv);
                    aout << "reconnection succeeded" << endl;
                    send_next_request();
                }
                catch (exception&) {
                    delayed_send(self, chrono::seconds(3), atom("reconnect"));
                }
            },
            on(atom("rebind"), arg_match) >> [=](string& host, uint16_t port) {
                actor_ptr new_serv;
                try {
                    new_serv = remote_actor(move(host), port);
                    self->monitor(new_serv);
                    aout << "rebind succeeded" << endl;
                    *serv = new_serv;
                    send_next_request();
                }
                catch (exception& e) {
                    aout << "*** rebind failed: "
                         << to_verbose_string(e) << endl;
                }
            },
            on(atom("quit")) >> [=] {
                self->quit();
            },
            others() >> [] {
                aout << "unexpected message: " << self->last_dequeued() << endl;
            }
        );
    }).spawn(server);
    aout << "quit                  Quit the program\n"
            "<x> + <y>             Calculate <x>+<y> and print result\n"
            "<x> - <y>             Calculate <x>-<y> and print result\n"
            "connect <host> <port> Reconfigure server"
         << endl;
    string line;
    const char connect[] = "connect ";
    while (getline(cin, line)) {
        trim(line);
        if (line == "quit") {
            send(client, atom("quit"));
            send(s_printer, atom("quit"));
            return;
        }
        // the STL way of line.starts_with("connect")
        else if (equal(begin(connect), end(connect) - 1, begin(line))) {
            match_split(line, ' ') (
                on("connect", val<string>, projection<string,uint16_t>) >> [&](string& host, uint16_t port) {
                    send(client, atom("rebind"), move(host), port);
                },
                others() >> [] {
                    aout << "illegal host/port definition" << endl;
                }
            );
        }
        else {
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
                    if (!lhs) aout << "\"" + lsub + "\" is not an integer" << endl;
                    if (!rhs) aout << "\"" + rsub + "\" is not an integer" << endl;
                }
            }
            else if (!success) aout << "*** invalid format; use: X +/- Y" << endl;
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
        print_desc(&desc, cerr)();
        return -1;
    }
    if (mode == "server") {
        try {
            // try to publish math actor at given port
            publish(spawn<math_actor>(), port);
        }
        catch (exception& e) {
            cerr << "*** unable to publish math actor at port " << port << "\n"
                 << to_verbose_string(e) // prints exception type and e.what()
                 << endl;
        }
    }
    else {
        if (host.empty()) host = "localhost";
        try {
            auto server = remote_actor(host, port);
            client_repl(server, host, port);
        }
        catch (exception& e) {
            cerr << "unable to connect to remote actor at host \""
                 << host << "\" on port " << port << "\n"
                 << to_verbose_string(e) << endl;
        }
    }
    await_all_others_done();
    shutdown();
    return 0;
}
