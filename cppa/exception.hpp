#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <string>
#include <cstdint>
#include <exception>

namespace cppa {

/**
 * @brief Base class for libcppa exceptions.
 */
class exception : public std::exception
{

    std::string m_what;

 protected:

    /**
     * @brief Creates an exception with the error string @p what_str.
     * @param what_str Error message as rvalue string.
     */
    exception(std::string&& what_str);

    /**
     * @brief Creates an exception with the error string @p what_str.
     * @param what_str Error message as string.
     */
    exception(const std::string& what_str);

 public:

    ~exception() throw();

    /**
     * @brief Returns the error message.
     * @returns The error message as C-string.
     */
    const char* what() const throw();

};

/**
 * @brief Thrown if an Actor finished execution.
 */
class actor_exited : public exception
{

    std::uint32_t m_reason;

 public:

    actor_exited(std::uint32_t exit_reason);

    /**
     *
     */
    inline std::uint32_t reason() const throw();

};

class network_exception : public exception
{

 public:

    network_exception(std::string&& what_str);
    network_exception(const std::string& what_str);

};

class bind_failure : public network_exception
{

    int m_errno;

 public:

    bind_failure(int bind_errno);

    inline int error_code() const throw();

};

inline std::uint32_t actor_exited::reason() const throw()
{
    return m_reason;
}

inline int bind_failure::error_code() const throw()
{
    return m_errno;
}

} // namespace cppa

#endif // EXCEPTION_HPP
