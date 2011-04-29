#ifndef DISABLE_IF_HPP
#define DISABLE_IF_HPP

namespace cppa { namespace util {

template<bool Stmt, typename T>
struct disable_if_c { };

template<typename T>
struct disable_if_c<false, T>
{
    typedef T type;
};

template<class Trait, typename T = void>
struct disable_if : disable_if_c<Trait::value, T>
{
};

} } // namespace cppa::util



#endif // DISABLE_IF_HPP
