#ifndef EXCEPTION_HPP
#define EXCEPTION_HPP

#include <string>
#include <cstdint>
#include <exception>

namespace cppa {

class exception : public std::exception
{

    std::string m_what;

 protected:

    exception(std::string&& what_str);
    exception(const std::string& what_str);

 public:

    ~exception() throw();
    const char* what() const throw();

};

class actor_exited : public exception
{

    std::uint32_t m_reason;

 public:

    actor_exited(std::uint32_t exit_reason);
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

    int error_code() const throw();

};

inline std::uint32_t actor_exited::reason() const throw()
{
    return m_reason;
}

} // namespace cppa

#endif // EXCEPTION_HPP
