#include <cstdio>
#include <fcntl.h>
#include <cstdint>
#include <cstring>      // strerror
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "cppa/detail/thread.hpp"
#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office.hpp"
#include "cppa/detail/post_office_msg.hpp"
#include "cppa/detail/network_manager.hpp"

namespace {

using namespace cppa;
using namespace cppa::detail;

struct network_manager_impl : network_manager
{

    typedef util::single_reader_queue<post_office_msg> post_office_queue_t;
    typedef util::single_reader_queue<mailman_job> mailman_queue_t;

    int m_pipe[2]; // m_pipe[0]: read; m_pipe[1]: write
    thread m_loop; // post office thread

    mailman_queue_t m_mailman_queue;
    post_office_queue_t m_post_office_queue;

    void start() /*override*/
    {
        if (pipe(m_pipe) != 0)
        {
            char* error_cstr = strerror(errno);
            std::string error_str = "pipe(): ";
            error_str += error_cstr;
            free(error_cstr);
            throw std::logic_error(error_str);
        }
        m_loop = thread(post_office_loop, m_pipe[0], m_pipe[1]);
    }

    int write_handle()
    {
        return m_pipe[1];
    }

    util::single_reader_queue<mailman_job>& mailman_queue()
    {
        return m_mailman_queue;
    }

    util::single_reader_queue<post_office_msg>& post_office_queue()
    {
        return m_post_office_queue;
    }

    void stop() /*override*/
    {
        pipe_msg msg = { shutdown_event, 0 };
        write(write_handle(), msg, pipe_msg_size);
        // m_loop calls close(m_pipe[0])
        m_loop.join();
        close(m_pipe[0]);
        close(m_pipe[1]);
    }

};

} // namespace <anonymous>

namespace cppa { namespace detail {

network_manager::~network_manager()
{
}

network_manager* network_manager::create_singleton()
{
    return new network_manager_impl;
}

} } // namespace cppa::detail
