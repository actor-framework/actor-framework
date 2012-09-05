#include <string>
#include <sstream>
#include <iostream>
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
    opt_strs.push_back(std::move(str));
    return on<string>().when(cppa::placeholders::_x1.in(opt_strs));
}

int main(int argc, char** argv) {
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
