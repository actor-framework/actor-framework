#ifndef INVOKE_HPP
#define INVOKE_HPP

#include <type_traits>

#include "cppa/get.hpp"
#include "cppa/tuple.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/tuple_view.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/pop_back.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/element_at.hpp"
#include "cppa/util/callable_trait.hpp"
#include "cppa/util/reverse_type_list.hpp"

namespace cppa { namespace detail {

template<size_t N, typename F, class Tuple,
         typename ArgTypeList, typename... Args>
struct invoke_helper
{
    typedef typename util::reverse_type_list<ArgTypeList>::type::head_type back_type;
    typedef typename util::element_at<N, Tuple>::type tuple_val_type;
    typedef typename util::pop_back<ArgTypeList>::type next_list;
    inline static void _(F& f, const Tuple& t, const Args&... args)
    {
        static_assert(std::is_convertible<tuple_val_type, back_type>::value,
                      "tuple element is not convertible to expected argument");
        invoke_helper<N - 1, F, Tuple, next_list, tuple_val_type, Args...>
                ::_(f, t, get<N>(t), args...);
    }
};

template<size_t N, typename F, class Tuple, typename... Args>
struct invoke_helper<N, F, Tuple, util::type_list<>, Args...>
{
    inline static void _(F& f, const Tuple&, const Args&... args)
    {
        f(args...);
    }
};

template<bool HasCallableTrati, typename F, class Tuple>
struct invoke_impl;

template<typename F, template<typename...> class Tuple, typename... TTypes>
struct invoke_impl<true, F, Tuple<TTypes...> >
{

    static_assert(sizeof...(TTypes) > 0, "empty tuple type");

    typedef typename util::callable_trait<F>::arg_types arg_types;

    typedef Tuple<TTypes...> tuple_type;

    inline static void _(F& f, const tuple_type& t)
    {
        invoke_helper<sizeof...(TTypes) - 1, F, tuple_type, arg_types>::_(f, t);
    }

};

template<typename F, template<typename...> class Tuple, typename... TTypes>
struct invoke_impl<false, F, Tuple<TTypes...> >
{

    static_assert(sizeof...(TTypes) > 0, "empty tuple type");

    typedef typename util::callable_trait<decltype(&F::operator())>::arg_types
            arg_types;

    typedef Tuple<TTypes...> tuple_type;

    inline static void _(F& f, const tuple_type& t)
    {
        invoke_helper<sizeof...(TTypes) - 1, F, tuple_type, arg_types>::_(f, t);
    }

};

} } // namespace cppa::detail

namespace cppa {

template<typename F, class Tuple>
void invoke(F what, const Tuple& args)
{
    typedef typename std::remove_pointer<F>::type f_type;
    detail::invoke_impl<std::is_function<f_type>::value, F, Tuple>::_(what, args);
}

} // namespace cppa

#endif // INVOKE_HPP
