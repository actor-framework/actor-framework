#include <set>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cstdlib>

#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;

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

auto on_void_opt(char short_opt, const char* long_opt) -> decltype(on<string>().when(cppa::placeholders::_x1.in(vector<string>()))) {
    const char short_flag_arr[] = {'-', short_opt, '\0' };
    vector<string> opt_strs = { short_flag_arr };
    opt_strs.push_back(string("-") + long_opt);
    string str = "-";
    str += opt_strs.back();
    opt_strs.push_back(move(str));
    return on<string>().when(cppa::placeholders::_x1.in(opt_strs));
}

class client : public event_based_actor {

    string    m_username;
    actor_ptr m_printer;

    void init() {
        become (
            // local commands
            on(atom("broadcast"), arg_match) >> [=](const string& message) {
                for(auto& dest : joined_groups()) {
                    send(dest, m_username + ": " + message);
                }
            },
            on(atom("join"), arg_match) >> [=](const group_ptr& what) {
                // accept join commands from local actors only
                auto s = self->last_sender();
                if (s->is_proxy()) {
                    reply("nice try");
                }
                else {
                    join(what);
                }
            },
            on(atom("quit")) >> [=] {
                auto s = self->last_sender();
                if (s->is_proxy()) {
                    reply("nice try");
                }
                else {
                    quit();
                }
            },
            on<string>() >> [=] {
                // don't print own messages
                if (last_sender() != this) forward_to(m_printer);
            },
            others() >> [=]() {
                send(m_printer, string("[!!!] unexpected message: '") + to_string(last_dequeued()));
            }
        );
    }

 public:

    client(const string& username, actor_ptr printer) : m_username(username), m_printer(printer) { }

};

class print_actor : public event_based_actor {
    void init() {
        become (
            on(atom("quit")) >> [] {
                self->quit();
            },
            on_arg_match >> [](const string& str) {
                cout << (str + "\n");
            }
        );
    }
};

inline vector<string> split(const string& str, char delim) {
    vector<string> result;
    stringstream strs{str};
    string tmp;
    while (getline(strs, tmp, delim)) result.push_back(tmp);
    return result;
}

template<typename Iterator>
inline string join(Iterator first, Iterator last, char delim) {
    string result;
    if (first != last) {
        result += *first++;
        for (; first != last; ++first) {
            result += delim;
            result += *first++;
        }
    }
    return result;
}

inline string join(const vector<string>& vec, char delim) {
    return join(begin(vec), end(vec), delim);
}

template<typename T>
auto conv(const string& str) -> option<T> {
    T result;
    if (istringstream(str) >> result) return result;
    return {};
}

void print_usage(actor_ptr printer) {
    send(printer, "Usage: group_chat --type=<server|client>\n --type, -t\t\tcan be: server, s, client, c\n --name, -n\t\tusername (only needed for client)\n --host, -h\t\thostname (only needed for client)\n --port, -p\t\tport for server/client");
}

function<option<string> (const string&)> get_extractor(const string& identifier) {
    auto tmp = [&](const string& kvp) -> option<string> {
        auto vec = split(kvp, '=');
        if (vec.size() == 2) {
            if (vec.front() == "--"+identifier) {
                return vec.back();
            }
        }
        return {};
    };
    return tmp;
}


auto main(int argc, char* argv[]) -> int {

    cout.unsetf(ios_base::unitbuf);

    auto printer = spawn<print_actor>();

    string name;

    bool args_valid = match_stream<string>(argv + 1, argv + argc) (
        on_opt('n', "name") >> [&](const string& input) -> bool {
            if (!name.empty()) {
                cout << "name already set to " << name << endl;
                return false;
            }
            name = input;
            return true;
        }
    );

    if(!args_valid) {
        print_usage(printer);
        send(printer, atom("quit"));
        await_all_others_done();
        shutdown();
        return 0;
    }

    while (name.empty()) {
        send(printer, "So what is your name for chatting?");
        if (!getline(cin, name)) return 1;
    }

    send(printer, "Starting client.");
    auto client_actor = spawn<client>(name, printer);

    auto get_command = [&]() -> bool {
        string input;
        bool done = false;
        getline(cin, input);
        if (input.size() > 0) {
            if (input.front() == '/') {
                input.erase(input.begin());
                vector<string> values = split(input, ' ');
                match (values) (
                    on("send", val<string>, any_vals) >> [&](const string& groupname) {
                        if (values.size() < 3) {
                            send(printer, "no message to send");
                        }
                        else {
                            send(client_actor, atom("send"), groupname, join(begin(values) + 2, end(values), ' '));
                        }
                    },
                    on("join", arg_match) >> [&](const string& mod, const string& id) {
                        group_ptr g;
                        try {
                            g = group::get(mod, id);
                            send(client_actor, atom("join"), g);
                        }
                        catch (exception& e) {
                            send(printer, string("exception: ") + e.what());
                        }
                    },
                    on("quit") >> [&] {
                        done = true;
                    },
                    others() >> [&] {
                        send(printer, "available commands:\n /connect HOST PORT\n /join GROUPNAME\n /join hamcast URI\n /send GROUPNAME MESSAGE\n /quit");
                    }
                );
            }
            else {
                send(client_actor, atom("broadcast"), input);
            }
        }
        return !done;
    };
    while (get_command()) { }
    send(client_actor, atom("quit"));

    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
