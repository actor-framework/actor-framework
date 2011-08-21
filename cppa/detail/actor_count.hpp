#ifndef ACTOR_COUNT_HPP
#define ACTOR_COUNT_HPP

#include <atomic>

#include <boost/thread.hpp>

#include "cppa/attachable.hpp"

namespace cppa { namespace detail {

class actor_count {

public:

    actor_count();

    void inc();
    void dec();

    // @pre expected <= 1
    void wait_until(size_t expected);

    static actor_count& get();

private:

    static actor_count* s_instance;

    boost::mutex m_ra_mtx;
    boost::condition_variable m_ra_cv;
    std::atomic<size_t> m_running_actors;
};

struct exit_observer : cppa::attachable
{
    virtual ~exit_observer();
};

} } // namespace cppa::detail

#endif // ACTOR_COUNT_HPP
