#ifndef ACTOR_EXITED_HPP
#define ACTOR_EXITED_HPP

#include <string>
#include <cstdint>
#include <exception>

namespace cppa {

class actor_exited : public std::exception
{

    std::uint32_t m_reason;
    std::string m_what;

 public:

    ~actor_exited() throw();
    actor_exited(std::uint32_t exit_reason);
    std::uint32_t reason() const;
    virtual const char* what() const throw();

};

} // namespace cppa

#endif // ACTOR_EXITED_HPP
