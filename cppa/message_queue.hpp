#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include "cppa/channel.hpp"

// forward declaration
namespace cppa { class invoke_rules; }

namespace cppa {

class message_queue
{

 public:

	virtual void enqueue(const message&) = 0;
	virtual const message& dequeue() = 0;
	virtual void dequeue(invoke_rules&) = 0;
	virtual bool try_dequeue(message&) = 0;
	virtual bool try_dequeue(invoke_rules&) = 0;
	virtual const message& last_dequeued() = 0;

};

} // namespace cppa

#endif // MESSAGE_QUEUE_HPP
