#ifndef TYPE_PLUGINS_HPP
#define TYPE_PLUGINS_HPP

#include <dlfcn.h>
#include <iostream>
#include "cppa/cppa.hpp"

inline void exec_plugin() {
    using namespace std;
    using namespace cppa;
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
}

#endif // TYPE_PLUGINS_HPP
