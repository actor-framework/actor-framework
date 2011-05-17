#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

// forward declarations
class actor;
class group;
class message;

class channel : public ref_counted
{

    friend class actor;
    friend class group;

    // channel is a sealed class and the only
    // allowed subclasses are actor and group.
    channel() = default;

 public:

    virtual ~channel();

    virtual void enqueue(const message&) = 0;

};

typedef intrusive_ptr<channel> channel_ptr;

} // namespace cppa

#endif // CHANNEL_HPP
