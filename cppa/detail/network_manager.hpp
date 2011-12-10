#ifndef NETWORK_MANAGER_HPP
#define NETWORK_MANAGER_HPP

#include "cppa/detail/mailman.hpp"
#include "cppa/detail/post_office_msg.hpp"

namespace cppa { namespace detail {

class network_manager
{

 public:

    virtual ~network_manager();

    virtual int write_handle() = 0;

    virtual void start() = 0;

    virtual void stop() = 0;

    virtual util::single_reader_queue<mailman_job>& mailman_queue() = 0;

    virtual util::single_reader_queue<post_office_msg>& post_office_queue() = 0;

    static network_manager* create_singleton();

};

} } // namespace cppa::detail

#endif // NETWORK_MANAGER_HPP
