#ifndef YIELD_INTERFACE_HPP
#define YIELD_INTERFACE_HPP

#include "cppa/util/fiber.hpp"

namespace cppa { namespace detail {

enum class yield_state
{
    // yield() wasn't called yet
    invalid,
    // actor is still ready
    ready,
    // actor waits for messages
    blocked,
    // actor finished execution
    done,
    // actor was killed because of an unhandled exception
    killed
};

// return to the scheduler / worker
void yield(yield_state);

// returns the yielded state of a scheduled actor
yield_state yielded_state();

// switches to @p what and returns to @p from after yield(...)
void call(util::fiber* what, util::fiber* from);

} } // namespace cppa::detail

#endif // YIELD_INTERFACE_HPP
