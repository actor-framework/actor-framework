#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "cppa/ref_counted.hpp"

namespace cppa { class message; }

namespace cppa { namespace detail {

// public part of the actor interface
struct channel : ref_counted
{
	virtual void enqueue_msg(const message& msg) = 0;
};

} } // namespace cppa::detail

#endif // CHANNEL_HPP
