#ifndef ACTOR_COUNT_HPP
#define ACTOR_COUNT_HPP

#include "cppa/attachable.hpp"

namespace cppa { namespace detail {

void inc_actor_count();
void dec_actor_count();

// @pre expected <= 1
void actor_count_wait_until(size_t expected);

struct exit_observer : cppa::attachable
{
    virtual ~exit_observer();
};

} } // namespace cppa::detail

#endif // ACTOR_COUNT_HPP
