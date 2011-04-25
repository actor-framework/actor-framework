#ifndef ENABLE_IF_HPP
#define ENABLE_IF_HPP

namespace cppa { namespace util {

template<bool Stmt, typename T = void>
struct enable_if_c { };

template<typename T>
struct enable_if_c<true, T>
{
	typedef T type;
};

template<class Trait, typename T = void>
struct enable_if : enable_if_c<Trait::value, T>
{
};

} } // namespace cppa::util

#endif // ENABLE_IF_HPP
