/******************************************************************************\
 * This example program represents a minimal IRC-like group                   *
 * communication server.                                                      *
 *                                                                            *
 * Setup for a minimal chat between "alice" and "bob":                        *
 * - ./build/bin/group_server -p 4242                                         *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n alice        *
 * - ./build/bin/group_chat -g remote:chatroom@localhost:4242 -n bob          *
\******************************************************************************/

#include <string>
#include <iostream>

#include "cppa/opt.hpp"
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;

int main(int argc, char** argv) {
    uint16_t port = 0;
    options_description desc;
    bool args_valid = match_stream<string>(argv + 1, argv + argc) (
        on_opt1('p', "port", &desc, "set port") >> rd_arg(port),
        on_opt0('h', "help", &desc, "print help") >> print_desc_and_exit(&desc)
    );
    if (port <= 1024) {
        cerr << "*** no port > 1024 given" << endl;
        args_valid = false;
    }
    if (!args_valid) {
        // print_desc(&desc) returns a function printing the stored help text
        auto desc_printer = print_desc(&desc);
        desc_printer();
        return 1;
    }
    try {
        // try to bind the group server to the given port,
        // this allows other nodes to access groups of this server via
        // group::get("remote", "<group>@<host>:<port>");
        // note: it is not needed to explicitly create a <group> on the server,
        //       as groups are created on-the-fly on first usage
        publish_local_groups_at(port);
    }
    catch (bind_failure& e) {
        // thrown if <port> is already in use
        cerr << "*** bind_failure: " << e.what() << endl;
        return 2;
    }
    catch (network_error& e) {
        // thrown on errors in the socket API
        cerr << "*** network error: " << e.what() << endl;
        return 2;
    }
    cout << "type 'quit' to shutdown the server" << endl;
    string line;
    while (getline(cin, line)) {
        if (line == "quit") return 0;
        else cout << "illegal command" << endl;
    }
    shutdown();
    return 0;
}
