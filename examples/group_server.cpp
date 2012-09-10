#include <string>
#include <dlfcn.h>
#include <sstream>
#include <iostream>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"
#include "type_plugins.hpp"

using namespace std;
using namespace cppa;

int main(int argc, char** argv) {
    // enables this server to be used with user-defined types
    // without changing this source code
    exec_plugin();
    uint16_t port = 0;
    options_description desc;
    bool args_valid = argc > 1 && match_stream<string>(argv + 1, argv + argc) (
        on_opt('p', "port", &desc, "set port") >> rd_arg(port),
        on_vopt('h', "help", &desc, "print help") >> print_desc_and_exit(&desc)
    );
    if (port <= 1024) {
        cout << "no port > 1024 given" << endl;
        return 2;
    }
    if (!args_valid) return 1;
    publish_local_groups_at(port);
    cout << "type 'quit' to shutdown the server" << endl;
    string line;
    while (getline(cin, line)) {
        if (line == "quit") return 0;
        else cout << "illegal command" << endl;
    }
    shutdown();
    return 0;
}
