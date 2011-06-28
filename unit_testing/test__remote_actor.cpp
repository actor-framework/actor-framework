#include <string>
#include <iostream>
#include <boost/thread.hpp>

#include "test.hpp"
#include "ping_pong.hpp"
#include "cppa/cppa.hpp"
#include "cppa/exception.hpp"

using std::cout;
using std::endl;

using namespace cppa;

namespace {

void client_part(const std::map<std::string, std::string>& args)
{
    auto i = args.find("port");
    if (i == args.end())
    {
        throw std::runtime_error("no port specified");
    }
    //(void) self();
    std::istringstream iss(i->second);
    std::uint16_t port;
    iss >> port;
    auto ping_actor = remote_actor("localhost", port);
    try
    {
        pong(ping_actor);
    }
    catch (cppa::actor_exited&)
    {
    }
    await_all_others_done();
}

} // namespace <anonymous>

size_t test__remote_actor(const char* app_path, bool is_client,
                          const std::map<std::string, std::string>& args)
{
    if (is_client)
    {
        client_part(args);
        return 0;
    }
    CPPA_TEST(test__remote_actor);
    auto ping_actor = spawn(ping);
    std::uint16_t port = 4242;
    bool success = false;
    do
    {
        try
        {
            publish(ping_actor, port);
            success = true;
        }
        catch (bind_failure&)
        {
            // try next port
            ++port;
        }
    }
    while (!success);
    //cout << "port = " << port << endl;
    std::string cmd;
    {
        std::ostringstream oss;
        oss << app_path << " run=remote_actor port=" << port;// << " &>/dev/null";
        cmd = oss.str();
    }
    // execute client_part() in a separate process,
    // connected via localhost socket
    boost::thread child([&cmd]() { system(cmd.c_str()); });
    await_all_others_done();
    CPPA_CHECK_EQUAL(pongs(), 5);
    // wait until separate process (in sep. thread) finished execution
    child.join();
    return CPPA_TEST_RESULT;
}
