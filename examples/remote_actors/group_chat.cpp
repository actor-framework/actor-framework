#include <set>
#include <map>
#include <vector>
#include <dlfcn.h>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cstdlib>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"
#include "type_plugins.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::placeholders;

struct line { string str; };

istream& operator>>(istream& is, line& l) {
    getline(is, l.str);
    return is;
}

any_tuple s_last_line;

any_tuple split_line(const line& l) {
    vector<string> result;
    stringstream strs(l.str);
    string tmp;
    while (getline(strs, tmp, ' ')) {
        if (!tmp.empty()) result.push_back(std::move(tmp));
    }
    s_last_line = any_tuple::view(std::move(result));
    return s_last_line;
}

class client : public event_based_actor {

 public:

    client(const string& name, actor_ptr printer)
    : m_name(name), m_printer(printer) { }

 protected:

    void init() {
        become (
            on(atom("broadcast"), arg_match) >> [=](const string& message) {
                for(auto& dest : joined_groups()) {
                    send(dest, m_name + ": " + message);
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
                // ignore quit messages from remote actors
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
                send(m_printer, to_string(last_dequeued()));
            }
        );
    }

 private:

    string    m_name;
    actor_ptr m_printer;

};

class print_actor : public event_based_actor {
    void init() {
        become (
            on(atom("quit")) >> [] {
                self->quit();
            },
            on_arg_match >> [](const string& str) {
                cout << str << endl;
            }
        );
    }
};

auto main(int argc, char* argv[]) -> int {

    // enables this client to print user-defined types
    // without changing this source code
    exec_plugin();

    string name;
    vector<string> group_ids;
    options_description desc;
    bool args_valid = argc > 1 && match_stream<string>(argv + 1, argv + argc) (
        on_opt('n', "name", &desc, "set name") >> rd_arg(name),
        on_opt('g', "group", &desc, "join group <arg>") >> add_arg(group_ids),
        on_vopt('h', "help", &desc, "print help") >> print_desc_and_exit(&desc)
    );

    if(!args_valid) print_desc_and_exit(&desc)();

    auto printer = spawn<print_actor>();

    while (name.empty()) {
        send(printer, "*** what is your name for chatting?");
        if (!getline(cin, name)) return 1;
    }

    send(printer, "*** starting client.");
    auto client_actor = spawn<client>(name, printer);

    // evaluate group parameters
    for (auto& gid : group_ids) {
        auto p = gid.find(':');
        if (p == std::string::npos) {
            cerr << "*** error parsing argument " << gid
                 << ", expected format: <module_name>:<group_id>";
        }
        else {
            try {
                auto g = group::get(gid.substr(0, p),
                                    gid.substr(p + 1));
                send(client_actor, atom("join"), g);
            }
            catch (exception& e) {
                ostringstream err;
                err << "*** exception: group::get(\"" << gid.substr(0, p)
                    << "\", \"" << gid.substr(p + 1) << "\") failed; "
                    << to_verbose_string(e) << endl;
                send(printer, err.str());
            }
        }
    }

    istream_iterator<line> lines(cin);
    istream_iterator<line> eof;
    match_each(lines, eof, split_line) (
        on("/join", arg_match) >> [&](const string& mod, const string& id) {
            try {
                send(client_actor, atom("join"), group::get(mod, id));
            }
            catch (exception& e) {
                send(printer, string("*** exception: ") + e.what());
            }
        },
        on("/quit") >> [&] {
            close(0); // close STDIN
        },
        on<string, anything>().when(_x1.starts_with("/")) >> [&] {
            send(printer, "*** available commands:\n "
                          "/join MODULE GROUP\n "
                          "/quit");
        },
        others() >> [&] {
            if (s_last_line.size() > 0) {
                string msg = s_last_line.get_as<string>(0);
                for (size_t i = 1; i < s_last_line.size(); ++i) {
                    msg += " ";
                    msg += s_last_line.get_as<string>(i);
                }
                send(client_actor, atom("broadcast"), msg);
            }
        }
    );
    send(client_actor, atom("quit"));
    send(printer, atom("quit"));
    await_all_others_done();
    shutdown();
    return 0;
}
