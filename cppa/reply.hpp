#ifndef REPLY_HPP
#define REPLY_HPP

#include "cppa/message.hpp"

namespace cppa {

template<typename... Args>
void reply(const Args&... args)
{
	auto whom = this_actor()->last_dequeued().sender();
	whom.send(args...);
}

} // namespace cppa

#endif // REPLY_HPP
