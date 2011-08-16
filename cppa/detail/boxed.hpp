#ifndef BOXED_HPP
#define BOXED_HPP

#include "cppa/any_type.hpp"
#include "cppa/util/wrapped.hpp"

namespace cppa { namespace detail {

template<typename T>
struct boxed
{
    typedef util::wrapped<T> type;
};

template<typename T>
struct boxed< util::wrapped<T> >
{
    typedef util::wrapped<T> type;
};

template<>
struct boxed<any_type>
{
    typedef any_type type;
};

template<>
struct boxed<any_type*>
{
    typedef any_type* type;
};

} } // namespace cppa::detail

#endif // BOXED_HPP
