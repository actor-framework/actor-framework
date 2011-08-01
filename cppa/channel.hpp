#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

// forward declarations
class actor;
class group;
class message;

/**
 * @brief The base interface for all message receivers.
 *
 * This interface is implemented by {@link actor} and {@link group} and
 * describes an entity that can receive messages.
 */
class channel : public ref_counted
{

    friend class actor;
    friend class group;

    // channel is a sealed class and the only
    // allowed subclasses are actor and group.
    channel() = default;

 public:

    virtual ~channel();

    /**
     * @brief Enqueues @p msg to the list of received messages.
     */
    virtual void enqueue(const message& msg) = 0;

};

/**
 * @brief A smart pointer type that manages instances of {@link channel}.
 */
typedef intrusive_ptr<channel> channel_ptr;

} // namespace cppa

#endif // CHANNEL_HPP
