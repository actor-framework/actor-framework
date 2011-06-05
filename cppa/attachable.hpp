#ifndef ATTACHABLE_HPP
#define ATTACHABLE_HPP

#include <cstdint>

namespace cppa {

// utilty class;
// provides a virtual destructor that's executed after an actor
// finished execution
/**
 * @brief Callback utility class.
 */
class attachable
{

 protected:

    attachable() = default;

 public:

    virtual ~attachable();

    /**
     * @brief Executed if the actor finished execution
     *        with given @p reason.
     *
     * The default implementation does nothing.
     */
    virtual void detach(std::uint32_t reason);

};

} // namespace cppa

#endif // ATTACHABLE_HPP
