#ifndef EXIT_SIGNAL_HPP
#define EXIT_SIGNAL_HPP

#include <cstdint>

namespace cppa {

enum class exit_reason : std::uint32_t
{

    /**
     * @brief Indicates that an actor finished execution.
     */
    normal = 0x00,

    /**
     * @brief Indicates that an actor finished execution
     *        because of an unhandled exception.
     */
    unhandled_exception = 0x01,

    /**
     * @brief Any user defined exit reason should have a
     *        value greater or equal to prevent collisions
     *        with default defined exit reasons.
     */
    user_defined = 0x10000

};

/**
 * @brief Converts {@link exit_reason} to @c std::uint32_t.
 */
constexpr std::uint32_t to_uint(exit_reason r)
{
    return static_cast<std::uint32_t>(r);
}

class exit_signal
{

    std::uint32_t m_reason;

 public:

    /**
     * @brief Creates an exit signal with
     *        <tt>reason() == exit_reason::normal</tt>.
     */
    exit_signal();

    /**
     * @brief Creates an exit signal with
     *        <tt>reason() == @p r</tt>.
     * @pre {@code r != exit_reason::user_defined}.
     */
    exit_signal(exit_reason r);

    /**
     * @brief Creates an exit signal with
     *        <tt>reason() == @p r</tt>.
     * @pre {@code r >= exit_reason::user_defined}.
     */
    exit_signal(std::uint32_t r);

    /**
     * @brief Reads the exit reason.
     */
    inline std::uint32_t reason() const
    {
        return m_reason;
    }

    // an unambiguous member function
    void set_uint_reason(std::uint32_t value);

    /**
     * @brief Sets the exit reason to @p value.
     */
    void set_reason(std::uint32_t value);

    /**
     * @brief Sets the exit reason to @p value.
     */
    void set_reason(exit_reason value);

};

} // namespace cppa

#endif // EXIT_SIGNAL_HPP
