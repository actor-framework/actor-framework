#ifndef EXIT_REASON_HPP
#define EXIT_REASON_HPP

#include <cstdint>

namespace cppa { namespace exit_reason {

/**
 * @brief Indicates that the actor is still alive.
 */
static constexpr std::uint32_t not_exited = 0x00000;

/**
 * @brief Indicates that an actor finished execution.
 */
static constexpr std::uint32_t normal = 0x00001;

/**
 * @brief Indicates that an actor finished execution
 *        because of an unhandled exception.
 */
static constexpr std::uint32_t unhandled_exception = 0x00002;

/**
 * @brief Indicates that an event-based actor
 *        tried to use receive().
 */
static constexpr std::uint32_t unallowed_function_call = 0x00003;

/**
 * @brief Indicates that an actor finishied execution
 *        because a connection to a remote link was
 *        closed unexpectedly.
 */
static constexpr std::uint32_t remote_link_unreachable = 0x00101;

/**
 * @brief Any user defined exit reason should have a
 *        value greater or equal to prevent collisions
 *        with default defined exit reasons.
 */
static constexpr std::uint32_t user_defined = 0x10000;

} } // namespace cppa::exit_reason

#endif // EXIT_REASON_HPP
