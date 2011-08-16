#ifndef WRAPPED_HPP
#define WRAPPED_HPP

namespace cppa { namespace util {

template<typename T>
struct wrapped
{
    typedef T type;
};

template<typename T>
struct wrapped< wrapped<T> >
{
    typedef typename wrapped<T>::type type;
};

} } // namespace cppa::util

#endif // WRAPPED_HPP
