#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

// forward declaration
class message;

class channel : public ref_counted
{

 public:

	virtual ~channel();

	virtual void enqueue(const message&) = 0;

};

typedef intrusive_ptr<channel> channel_ptr;

} // namespace cppa

#endif // CHANNEL_HPP
