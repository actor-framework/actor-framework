#ifndef UNBOXED_HPP
#define UNBOXED_HPP

#include "cppa/any_type.hpp"
#include "cppa/util/wrapped.hpp"

namespace cppa { namespace detail {

template<typename T>
struct unboxed
{
    typedef T type;
};

template<typename T>
struct unboxed< util::wrapped<T> >
{
    typedef typename util::wrapped<T>::type type;
};

} } // namespace cppa::detail

#endif // UNBOXED_HPP
