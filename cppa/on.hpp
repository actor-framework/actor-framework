#ifndef ON_HPP
#define ON_HPP

#include <chrono>
#include <memory>

#include "cppa/atom.hpp"
#include "cppa/invoke.hpp"
#include "cppa/pattern.hpp"
#include "cppa/anything.hpp"
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

    typedef pattern<TypeList...> pattern_type;
    typedef std::unique_ptr<pattern_type> pattern_ptr_type;

    pattern_ptr_type m_pattern;

    typedef cppa::util::type_list<TypeList...> plain_types;

    typedef typename cppa::util::filter_type_list<anything, plain_types>::type
            filtered_types;

    typedef typename cppa::tuple_view_type_from_type_list<filtered_types>::type
            tuple_view_type;

 public:

    template<typename... Args>
    invoke_rule_builder(const Args&... args)
    {
        m_pattern.reset(new pattern_type(args...));
    }

    template<typename F>
    invoke_rules operator>>(F&& f)
    {
        typedef invokable_impl<tuple_view_type, pattern_type, F> impl;
        return invokable_ptr(new impl(std::move(m_pattern),std::forward<F>(f)));
    }

};

} } // cppa::detail

namespace cppa {

template<typename T>
constexpr typename detail::boxed<T>::type val()
{
    return typename detail::boxed<T>::type();
}

template<typename Arg0, typename... Args>
detail::invoke_rule_builder<typename detail::unboxed<Arg0>::type,
                            typename detail::unboxed<Args>::type...>
on(const Arg0& arg0, const Args&... args)
{
    return { arg0, args... };
    /*
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
    */
}

template<typename... TypeList>
detail::invoke_rule_builder<TypeList...> on()
{
    return { };
    /*
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<TypeList...>(data, mv);
        }
    };
    */
}

template<atom_value A0, typename... TypeList>
detail::invoke_rule_builder<atom_value, TypeList...> on()
{
    return { A0 };
    /*
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, TypeList...>(data, mv, A0);
        }
    };
    */
}

template<atom_value A0, atom_value A1, typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value, TypeList...> on()
{
    return { A0, A1 };
    /*
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, atom_value, TypeList...>(data, mv, A0, A1);
        }
    };
    */
}

template<atom_value A0, atom_value A1, atom_value A2, typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value,
                            atom_value, TypeList...> on()
{
    return { A0, A1, A2 };
    /*
    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, atom_value,
                         atom_value, TypeList...>(data, mv, A0, A1, A2);
        }
    };
    */
}

template<atom_value A0, atom_value A1,
         atom_value A2, atom_value A3,
         typename... TypeList>
detail::invoke_rule_builder<atom_value, atom_value, atom_value,
                            atom_value, TypeList...> on()
{
    return { A0, A1, A2, A3 };
    /*

    return
    {
        [](const any_tuple& data, std::vector<size_t>* mv) -> bool
        {
            return match<atom_value, atom_value, atom_value,
                         atom_value, TypeList...>(data, mv, A0, A1, A2, A3);
        }
    };
    */
}

template<class Rep, class Period>
inline constexpr detail::timed_invoke_rule_builder
after(const std::chrono::duration<Rep, Period>& d)
{
    return { util::duration(d) };
}

inline detail::invoke_rule_builder<anything> others()
{
    return { };
}

} // namespace cppa

#endif // ON_HPP
