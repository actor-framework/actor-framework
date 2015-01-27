/******************************************************************************\
 * This example program represents a minimal GUI chat program                 *
 * based on group communication. This chat program is compatible to the       *
 * terminal version in remote_actors/group_chat.cpp.                          *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/bin/group_server -p 4242                                         *
 * - ./build/bin/qt_group_chat -g remote:chatroom@localhost:4242 -n alice     *
 * - ./build/bin/qt_group_chat -g remote:chatroom@localhost:4242 -n bob       *
\******************************************************************************/

#include <set>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <time.h>
#include <cstdlib>

#include <QMainWindow>
#include <QApplication>

#include "caf/all.hpp"
#include "cppa/opt.hpp"

#include "ui_chatwindow.h" // auto generated from chatwindow.ui

using namespace std;
using namespace caf;

/*
namespace {

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

} // namespace <anonymous>
*/

int main(int argc, char** argv) {

    string name;
    string group_id;
    options_description desc;
    bool args_valid = match_stream<string>(argv + 1, argv + argc) (
        on_opt1('n', "name", &desc, "set name") >> rd_arg(name),
        on_opt1('g', "group", &desc, "join group <arg1>") >> rd_arg(group_id),
        on_opt0('h', "help", &desc, "print help") >> print_desc_and_exit(&desc)
    );

    group gptr;
    // evaluate group parameter
    if (!group_id.empty()) {
        auto p = group_id.find(':');
        if (p == std::string::npos) {
            cerr << "*** error parsing argument " << group_id
                 << ", expected format: <module_name>:<group_id>";
        }
        else {
            try {
                gptr = group::get(group_id.substr(0, p),
                                  group_id.substr(p + 1));
            }
            catch (exception& e) {
                ostringstream err;
                cerr << "*** exception: group::get(\"" << group_id.substr(0, p)
                     << "\", \"" << group_id.substr(p + 1) << "\") failed; "
                     << to_verbose_string(e) << endl;
            }
        }
    }

    if (!args_valid) print_desc_and_exit(&desc)();

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(true);
    QMainWindow mw;
    Ui::ChatWindow helper;
    helper.setupUi(&mw);
    auto client = helper.chatwidget->as_actor();
    if (!name.empty()) {
        send_as(client, client, atom("setName"), move(name));
    }
    if (gptr) {
        send_as(client, client, atom("join"), gptr);
    }
    mw.show();
    auto res = app.exec();
    shutdown();
    await_all_actors_done();
    return res;
}
