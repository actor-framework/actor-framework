#include "cppa/cppa.hpp"

namespace cppa {

context* operator<<(context* whom, const any_tuple& what)
{
    if (whom) whom->enqueue(message(self(), whom, what));
    return whom;
}

// matches self() << make_tuple(...)
context* operator<<(context* whom, any_tuple&& what)
{
    if (whom) whom->enqueue(message(self(), whom, std::move(what)));
    return whom;
}

} // namespace cppa
