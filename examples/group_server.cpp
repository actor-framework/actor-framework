#include <string>
#include <dlfcn.h>
#include <sstream>
#include <iostream>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;

int main(int argc, char** argv) {

    // user-defined types can be announced by a plugin
    void* handle = dlopen("plugin.dylib", RTLD_NOW);     // macos
    if (!handle) handle = dlopen("plugin.so", RTLD_NOW); // linux
    if (handle) {
        auto before = uniform_type_info::instances();
        cout << "found a plugin, call exec_plugin()" << endl;
        auto fun = (void (*)()) dlsym(handle, "exec_plugin");
        if (fun) {
            fun();
            cout << "the plugin announced the following types:" << endl;
            for (auto inf : uniform_type_info::instances()) {
                if (count(begin(before), end(before), inf) == 0) {
                    cout << inf->name() << endl;
                }
            }
        }
    }

    uint16_t port = 0;
    bool args_valid = argc > 1 && match_stream<string>(argv + 1, argv + argc) (
        on_opt('p', "port") >> [&](const string& arg) -> bool {
            if (!(istringstream(arg) >> port)) {
                cout << "\"" << arg << "\" is not a valid port" << endl;
                return false;
            }
            return true;
        }
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
        else {
            cout << "illegal command" << endl;
        }
    }
    return 0;
}
