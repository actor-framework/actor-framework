#ifndef ACTOR_COUNT_HPP
#define ACTOR_COUNT_HPP

namespace cppa { namespace detail {

void inc_actor_count();
void dec_actor_count();

// @pre expected <= 1
void actor_count_wait_until(size_t expected);

} } // namespace cppa::detail

#endif // ACTOR_COUNT_HPP
