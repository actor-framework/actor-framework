#ifndef ON_HPP
#define ON_HPP

#include <chrono>

#include "cppa/atom.hpp"
#include "cppa/match.hpp"
#include "cppa/invoke.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/invoke_rules.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/eval_first_n.hpp"
#include "cppa/util/filter_type_list.hpp"

#include "cppa/detail/boxed.hpp"
#include "cppa/detail/unboxed.hpp"
#include "cppa/detail/invokable.hpp"
#include "cppa/detail/ref_counted_impl.hpp"

namespace cppa { namespace detail {

class timed_invoke_rule_builder
{

    util::duration m_timeout;

 public:

    constexpr timed_invoke_rule_builder(const util::duration& d) : m_timeout(d)
    {
    }

    template<typename F>
    timed_invoke_rules operator>>(F&& f)
    {
        typedef timed_invokable_impl<F> impl;
        return timed_invokable_ptr(new impl(m_timeout, std::forward<F>(f)));
    }

};

template<typename... TypeList>
class invoke_rule_builder
{

    typedef std::function<bool (const any_tuple&, std::vector<size_t>*)>
            match_function;

    match_function m_fun;

    typedef cppa::util::type_list<TypeList...> plain_types;

    typedef typename cppa::util::filter_type_list<any_type, plain_types>::type
            filtered_types;

    typedef typename cppa::tuple_view_type_from_type_list<filtered_types>::type
            tuple_view_type;

 public:

    invoke_rule_builder(match_function&& fun) : m_fun(std::move(fun)) { }

    template<typename F>
    invoke_rules operator>>(F&& f)
    {
        typedef invokable_impl<tuple_view_type, match_function, F> impl;
        return invokable_ptr(new impl(std::move(m_fun), std::forward<F>(f)));
    }

};

template<class MatchedTypes, class ArgTypes>
struct match_util;

template<typename... MatchedTypes, typename... ArgTypes>
struct match_util<util::type_list<MatchedTypes...>,
                  util::type_list<ArgTypes...>>
{
    static bool _(const any_tuple& data, std::vector<size_t>* mapping,
                  const ArgTypes&... args)
    {
        return match<MatchedTypes...>(data, mapping, args...);
    }
};

} } // cppa::detail

namespace cppa {

template<typename T = any_type>
constexpr typename detail::boxed<T>::type val()
{
    return typename detail::boxed<T>::type();
}

#ifdef __GNUC__
constexpr any_type* any_vals __attribute__ ((unused)) = nullptr;
#else
constexpr any_type* any_vals = nullptr;
#endif

template<typename Arg0, typename... Args>
detail::invoke_rule_builder<typename detail::unboxed<Arg0>::type,
                            typename detail::unboxed<Args>::type...>
on(const Arg0& arg0, const Args&... args)
{
    typedef util::type_list<typename detail::unboxed<Arg0>::type,
                            typename detail::unboxed<Args>::type...> mt;

    typedef util::type_list<Arg0, Args...> vt;

    typedef detail::tdata<any_tuple, std::vector<size_t>*, Arg0 , Args...>
            arg_tuple_type;

    arg_tuple_type arg_tuple(any_tuple(), nullptr, arg0, args...);

    return
    {
        [arg_tuple](const any_tuple& data, std::vector<size_t>* mapping) -> bool
        {
            arg_tuple_type& tref = const_cast<arg_tuple_type&>(arg_tuple);
            cppa::get_ref<0>(tref) = data;
            cppa::get_ref<1>(tref) = mapping;
            return invoke(&detail::match_util<mt,vt>::_, tref);
        }
    };
}

template<typename... TypeList>
detail::invoke_rule_builder<TypeList...> on()
{
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<TypeList...>(data, mv);
        }
    };
}

template<atom_value A0, typename... TypeList>
detail::invoke_rule_builder<atom_value, TypeList...> on()
{
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, TypeList...>(data, mv, A0);
        }
    };
}

template<atom_value A0, atom_value A1, typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value, TypeList...> on()
{
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, atom_value, TypeList...>(data, mv, A0, A1);
        }
    };
}

template<atom_value A0, atom_value A1, atom_value A2, typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value,
                            atom_value, TypeList...> on()
{
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, atom_value,
                         atom_value, TypeList...>(data, mv, A0, A1, A2);
        }
    };
}

template<atom_value A0, atom_value A1,
         atom_value A2, atom_value A3,
         typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value, atom_value,
                            atom_value, TypeList...> on()
{
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, atom_value, atom_value,
                         atom_value, TypeList...>(data, mv, A0, A1, A2, A3);
        }
    };
}

template<class Rep, class Period>
inline constexpr detail::timed_invoke_rule_builder
after(const std::chrono::duration<Rep, Period>& d)
{
    return { util::duration(d) };
}

inline detail::invoke_rule_builder<any_type*> others()
{
    return
    {
        [](const any_tuple&, std::vector<size_t>*) -> bool
        {
            return true;
        }
    };
}

} // namespace cppa

#endif // ON_HPP
