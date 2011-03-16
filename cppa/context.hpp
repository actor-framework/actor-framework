#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "cppa/actor.hpp"
#include "cppa/message_queue.hpp"

namespace cppa {

class context : public actor
{

 public:

	virtual message_queue& mailbox() = 0;

	virtual void unlink(const actor_ptr& other) = 0;

};

} // namespace cppa

#endif // CONTEXT_HPP
