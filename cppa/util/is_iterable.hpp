#ifndef IS_ITERABLE_HPP
#define IS_ITERABLE_HPP

#include "cppa/util/is_primitive.hpp"
#include "cppa/util/is_forward_iterator.hpp"

namespace cppa { namespace util {

template<typename T>
class is_iterable
{

    // this horrible code would just disappear if we had concepts
    template<class C>
    static bool sfinae_fun
    (
        C const* cc,
        // check for 'C::begin()' returning a forward iterator
        typename util::enable_if<util::is_forward_iterator<decltype(cc->begin())>>::type* = 0,
        // check for 'C::end()' returning the same kind of forward iterator
        typename util::enable_if<std::is_same<decltype(cc->begin()), decltype(cc->end())>>::type* = 0
    )
    {
        return true;
    }

    // SFNINAE default
    static void sfinae_fun(...) { }

    typedef decltype(sfinae_fun(static_cast<T const*>(nullptr))) result_type;

 public:

    static const bool value =    util::is_primitive<T>::value == false
                              && std::is_same<bool, result_type>::value;

};

} } // namespace cppa::util

#endif // IS_ITERABLE_HPP
