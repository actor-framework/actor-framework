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

template<size_t N, typename F, typename ResultType, class Tuple,
         typename ArgTypeList, typename... Args>
struct invoke_helper
{
    typedef typename util::reverse_type_list<ArgTypeList>::type rlist;
    typedef typename rlist::head_type back_type;
    typedef typename util::element_at<N, Tuple>::type tuple_val_type;
    typedef typename util::pop_back<ArgTypeList>::type next_list;
    inline static ResultType _(F& f, Tuple const& t, Args const&... args)
    {
        static_assert(std::is_convertible<tuple_val_type, back_type>::value,
                      "tuple element is not convertible to expected argument");
        return invoke_helper<N - 1, F, ResultType, Tuple, next_list, tuple_val_type, Args...>
                ::_(f, t, get<N>(t), args...);
    }
};

template<size_t N, typename F, typename ResultType, class Tuple, typename... Args>
struct invoke_helper<N, F, ResultType, Tuple, util::type_list<>, Args...>
{
    inline static ResultType _(F& f, Tuple const&, Args const&... args)
    {
        return f(args...);
    }
};

template<bool HasCallableTrait, typename F, class Tuple>
struct invoke_impl;

template<typename F, template<typename...> class Tuple, typename... TTypes>
struct invoke_impl<true, F, Tuple<TTypes...> >
{

    static_assert(sizeof...(TTypes) > 0, "empty tuple type");

    typedef util::callable_trait<F> trait;

    typedef typename trait::arg_types arg_types;
    typedef typename trait::result_type result_type;

    typedef Tuple<TTypes...> tuple_type;

    inline static result_type _(F& f, tuple_type const& t)
    {
        return invoke_helper<sizeof...(TTypes) - 1, F, result_type, tuple_type, arg_types>::_(f, t);
    }

};

template<typename F, template<typename...> class Tuple, typename... TTypes>
struct invoke_impl<false, F, Tuple<TTypes...> >
{

    static_assert(sizeof...(TTypes) > 0, "empty tuple type");

    typedef util::callable_trait<decltype(&F::operator())> trait;

    typedef typename trait::arg_types arg_types;
    typedef typename trait::result_type result_type;

    typedef Tuple<TTypes...> tuple_type;

    inline static result_type _(F& f, tuple_type const& t)
    {
        return invoke_helper<sizeof...(TTypes) - 1, F, result_type, tuple_type, arg_types>::_(f, t);
    }

};

} } // namespace cppa::detail

namespace cppa {

template<typename F, class Tuple>
typename detail::invoke_impl<std::is_function<typename std::remove_pointer<F>::type>::value, F, Tuple>::result_type
invoke(F what, Tuple const& args)
{
    typedef typename std::remove_pointer<F>::type f_type;
    static constexpr bool is_fun = std::is_function<f_type>::value;
    return detail::invoke_impl<is_fun, F, Tuple>::_(what, args);
}

} // namespace cppa

#endif // INVOKE_HPP
