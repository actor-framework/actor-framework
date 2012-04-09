#include <list>
#include <string>
#include <utility>
#include <iostream>
#include <typeinfo>
#include <functional>
#include <type_traits>

#include "test.hpp"

#include "cppa/on.hpp"
#include "cppa/cow_tuple.hpp"
#include "cppa/pattern.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/to_string.hpp"
#include "cppa/tuple_cast.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"

#include <cxxabi.h>

using std::cout;
using std::endl;

using namespace cppa;

template<class Expr, class Guard, typename Result, typename... Args>
class tpartial_function
{

    bool collect(detail::tdata<>&) { return true; }

    template<typename TData, typename T0, typename... Ts>
    bool collect(TData& td, T0 const& arg0, Ts const&... args)
    {
        td.head = arg0;
        return collect(td.tail(), args...);
    }

 public:

    tpartial_function(Guard&& g, Expr&& e)
        : m_guard(std::move(g)), m_expr(std::move(e))
    {
    }

    tpartial_function(tpartial_function&& other)
        : m_guard(std::move(other.m_guard))
        , m_expr(std::move(other.m_expr))
    {
    }

    tpartial_function(tpartial_function const&) = default;

    bool defined_at(Args const&... args)
    {
        return m_guard(args...);
    }

    Result operator()(Args const&... args)
    {
        if (collect(m_args, args...)) return m_expr(args...);
        throw std::logic_error("oops");
    }

 private:

    typedef util::type_list<ge_reference_wrapper<Args>...> TransformedArgs;

    typename detail::tdata_from_type_list<TransformedArgs>::type m_args;
    Guard m_guard;
    Expr m_expr;

};

template<typename T>
T const& fw(T const& value) { return value; }

template<typename T>
struct rm_option
{
    typedef T type;
};

template<typename T>
struct rm_option<option<T> >
{
    typedef T type;
};

template<typename ArgType, typename Transformer>
struct cf_transformed_type
{
    typedef typename util::get_callable_trait<Transformer>::result_type result;
    typedef typename rm_option<result>::type type;
};

template<typename ArgType>
struct cf_transformed_type<ArgType, util::void_type>
{
    typedef ge_reference_wrapper<ArgType> type;
};

struct invoke_policy_helper
{
    size_t i;
    detail::abstract_tuple const& tup;
    invoke_policy_helper(detail::abstract_tuple const& tp) : i(0), tup(tp) { }
    template<typename T>
    void operator()(ge_reference_wrapper<T>& storage)
    {
        storage = *reinterpret_cast<T const*>(tup.at(i++));
    }
};

template<wildcard_position, class ArgsList>
struct invoke_policy;

template<typename... Args>
struct invoke_policy<wildcard_position::nil, util::type_list<Args...> >
{
    template<typename Target>
    static bool invoke_args(Target& target, Args const&... args)
    {
        return target(args...);
    }
    template<typename Target, typename... WrongArgs>
    static bool invoke_args(Target&, WrongArgs const&...)
    {
        return false;
    }
    template<typename Target>
    static bool invoke(Target& target,
                       std::type_info const& arg_types,
                       detail::tuple_impl_info timpl,
                       void const* native_arg,
                       detail::abstract_tuple const& tup)
    {
        if (arg_types == typeid(util::type_list<Args...>))
        {
            if (native_arg)
            {
                auto arg = reinterpret_cast<detail::tdata<Args...> const*>(native_arg);
                return util::unchecked_apply_tuple<bool>(target, *arg);
            }
            // 'fall through'
        }
        else if (timpl == detail::dynamically_typed)
        {
            auto& arr = detail::static_types_array<Args...>::arr;
            if (tup.size() != sizeof...(Args))
            {
                return false;
            }
            for (size_t i = 0; i < sizeof...(Args); ++i)
            {
                if (arr[i] != tup.type_at(i))
                {
                    return false;
                }
            }
            // 'fall through'
        }
        else
        {
            return false;
        }
        // either dynamically typed or statically typed but not a native tuple
        detail::tdata<ge_reference_wrapper<Args>...> ttup;
        invoke_policy_helper iph{tup};
        util::static_foreach<0, sizeof...(Args)>::_ref(ttup, iph);
        return util::apply_tuple(target, ttup);
    }
};

template<class Transformers, class Args>
class apply_policy
{

 public:

    template<typename... Ts>
    apply_policy(Ts&&... args) : m_transformer(std::forward<Ts>(args)...) { }

    apply_policy(apply_policy&&) = default;
    apply_policy(apply_policy const&) = default;

    template<class Guard, class Expr, typename... Ts>
    bool operator()(Guard& guard, Expr& expr, Ts const&... args) const
    {
        typename detail::tdata_from_type_list<
            typename util::tl_zipped_apply<
                typename util::tl_zip<
                    Args,
                    typename util::tl_resize<
                        Transformers,
                        Args::size,
                        util::void_type
                    >::type
                >::type,
                cf_transformed_type
            >::type
        >::type
        m_args;
        if (collect(m_args, m_transformer, args...))
        {
            if (util::unchecked_apply_tuple<bool>(guard, m_args))
            {
                util::apply_tuple(expr, m_args);
                return true;
            }
        }
        return false;
    }

 private:

    template<class Storage, typename T>
    static inline  bool fetch_(Storage& storage, T const& value)
    {
        storage = value;
        return true;
    }

    template<class Result>
    static inline  bool fetch_(Result& storage, option<Result>&& value)
    {
        if (value)
        {
            storage = std::move(*value);
            return true;
        }
        return false;
    }

    template<class Storage, typename Fun, typename T>
    static inline  bool fetch(Storage& storage, Fun const& fun, T const& arg)
    {
        return fetch_(storage, fun(arg));
    }

    template<typename T>
    static inline  bool fetch(ge_reference_wrapper<T>& storage,
                              util::void_type const&,
                              T const& arg)
    {
        storage = arg;
        return true;
    }

    static inline bool collect(detail::tdata<>&, detail::tdata<> const&) { return true; }

    template<class TData, typename T0, typename... Ts>
    static inline bool collect(TData& td, detail::tdata<> const&, T0 const& arg0, Ts const&... args)
    {
        td.set(arg0, args...);
        return true;
    }

    template<class TData, class Trans, typename T0, typename... Ts>
    static inline bool collect(TData& td, Trans const& tr, T0 const& arg0, Ts const&... args)
    {
        return    fetch(td.head, tr.head, arg0)
               && collect(td.tail(), tr.tail(), args...);
    }

    typename detail::tdata_from_type_list<Transformers>::type m_transformer;

};

template<typename Expr, size_t ExprArgs, size_t NumArgs>
struct apply_args
{
    template<typename Arg0, typename... Args>
    static inline void _(Expr& expr, Arg0 const&, Args const&... args)
    {
        apply_args<Expr, ExprArgs, NumArgs-1>::_(expr, args...);
    }
};

template<typename Expr, size_t Num>
struct apply_args<Expr, Num, Num>
{
    template<typename... Args>
    static inline void _(Expr& expr, Args const&... args)
    {
        expr(args...);
    }
};

template<class Args>
class apply_policy<util::type_list<>, Args>
{

 public:

    apply_policy(detail::tdata<> const&) { }

    apply_policy() = default;
    apply_policy(apply_policy const&) = default;

    template<class Guard, class Expr, typename... Ts>
    bool operator()(Guard& guard, Expr& expr, Ts const&... args) const
    {
        if (guard(args...))
        {
            typedef typename util::get_callable_trait<Expr>::type ctrait;
            apply_args<Expr, ctrait::arg_types::size, sizeof...(Ts)>::_(expr, args...);
            return true;
        }
        return false;
    }

};

template<class InvokePolicy>
struct invoke_helper_
{
    template<class Leaf>
    bool operator()(Leaf const& leaf,
                    std::type_info const& v0,
                    detail::tuple_impl_info v1,
                    void const* v2,
                    detail::abstract_tuple const& v3) const
    {
        return InvokePolicy::invoke(leaf, v0, v1, v2, v3);
    }
    template<class Leaf, typename... Args>
    bool operator()(Leaf const& leaf, Args const&... args) const
    {
        return InvokePolicy::invoke_args(leaf, args...);
    }
};

struct invoke_helper
{
    template<class ArgTypes, class... Leaves, typename... Args>
    bool operator()(detail::tdata<ArgTypes, Leaves...> const& list,
                    Args const&... args) const
    {
        typedef invoke_policy<get_wildcard_position<ArgTypes>(), ArgTypes> ipolicy;
        invoke_helper_<ipolicy> fun;
        return util::static_foreach<1, sizeof...(Leaves)+1>::eval_or(list, fun, args...);
    }
};

template<class Expr, class Guard, class Transformers, class ArgTypes>
class conditional_fun_leaf;

template<class Expr, class Guard, class Transformers, typename... Args>
class conditional_fun_leaf<Expr, Guard, Transformers, util::type_list<Args...> >
{

 public:

    typedef util::type_list<Args...> args_list;

    template<typename G, typename E, typename... TFuns>
    conditional_fun_leaf(G&& g, E&& e, TFuns&&... tfuns)
        : m_apply(std::forward<TFuns>(tfuns)...)
        , m_guard(std::forward<G>(g)), m_expr(std::forward<E>(e))
    {
    }

    conditional_fun_leaf(conditional_fun_leaf const&) = default;

    bool operator()(Args const&... args) const
    {
        return m_apply(m_guard, m_expr, args...);
    }

 private:

    typedef typename util::tl_trim<Transformers, util::void_type>::type trimmed;
    apply_policy<trimmed, args_list> m_apply;
    Guard m_guard;
    Expr m_expr;

};

template<class ArgTypes, class Expr, class Guard, typename... TFuns>
struct cfl_
{
    typedef conditional_fun_leaf<Expr, Guard,
                                 util::type_list<TFuns...>, ArgTypes>
            type;
};

template<class ArgTypes, class LeavesTData, class... LeafImpl>
struct cfh_
{
    typedef LeavesTData leaves_type;                   // tdata<tdata...>
    typedef typename leaves_type::back_type back_leaf; // tdata

    // manipulate leaves type to push back leaf_impl
    typedef typename util::tl_pop_back<typename leaves_type::types>::type
            prefix;
            // type_list<tdata...>

    typedef typename detail::tdata_from_type_list<
                typename util::tl_concat<
                    typename back_leaf::types,
                    util::type_list<LeafImpl...>
                >::type
            >::type
            suffix; // tdata

    typedef typename util::tl_push_back<prefix, suffix>::type
            concatenated; // type_list<tdata...>

    typedef typename detail::tdata_from_type_list<concatenated>::type type;
};

namespace cppa { namespace detail {

template<class Lhs, class Rhs>
struct tdata_concatenate;

template<class... Lhs, class... Rhs>
struct tdata_concatenate<tdata<Lhs...>, tdata<Rhs...> >
{
    typedef tdata<Lhs..., Rhs...> type;
};

} } // namespace cppa::detail

template<class Leaves>
class conditional_fun
{

 public:

    template<typename... Args>
    conditional_fun(Args&&... args) : m_leaves(std::forward<Args>(args)...)
    {
    }

    conditional_fun(conditional_fun&& other) : m_leaves(std::move(other.m_leaves))
    {
    }

    conditional_fun(conditional_fun const&) = default;

    //conditional_fun& operator=(conditional_fun&&) = default;
    //conditional_fun& operator=(conditional_fun const&) = default;

    typedef void const* const_void_ptr;

    bool invoke(std::type_info const& arg_types,
                detail::tuple_impl_info timpl,
                const_void_ptr native_arg,
                detail::abstract_tuple const& tup) const
    {
        invoke_helper fun;
        return util::static_foreach<0, Leaves::tdata_size>::eval_or(m_leaves, fun, arg_types, timpl, native_arg, tup);
    }

    bool invoke(any_tuple const& tup) const
    {
        auto const& cvals = *(tup.cvals());
        return invoke(*(cvals.type_token()),
                      cvals.impl_type(),
                      cvals.native_data(),
                      cvals);
    }

    template<typename... Args>
    bool operator()(Args const&... args)
    {
        invoke_helper fun;
        return util::static_foreach<0, Leaves::tdata_size>::eval_or(m_leaves, fun, args...);
    }

    typedef typename Leaves::back_type back_leaf;
    typedef typename back_leaf::head_type back_arg_types;

    template<typename OtherLeaves>
    conditional_fun<typename detail::tdata_concatenate<Leaves, OtherLeaves>::type>
    or_else(conditional_fun<OtherLeaves> const& other) const
    {
        return {m_leaves, other.m_leaves};
    }

    template<class... LeafImpl>
    conditional_fun<typename cfh_<back_arg_types, Leaves, LeafImpl...>::type>
    or_else(conditional_fun<detail::tdata<detail::tdata<back_arg_types, LeafImpl...>>> const& other) const
    {
        return {m_leaves, other.m_leaves.back().tail()};
    }

    template<class... LeafImpl, class Others0, class... Others>
    conditional_fun<
        typename detail::tdata_concatenate<
            typename cfh_<back_arg_types, Leaves, LeafImpl...>::type,
            detail::tdata<Others0, Others...>
        >::type>
    or_else(conditional_fun<detail::tdata<detail::tdata<back_arg_types, LeafImpl...>, Others0, Others...>> const& other) const
    {
        typedef conditional_fun<detail::tdata<detail::tdata<back_arg_types, LeafImpl...>>>
                head_fun;
        typedef conditional_fun<detail::tdata<Others0, Others...>> tail_fun;
        return or_else(head_fun{other.m_leaves.head})
              .or_else(tail_fun{other.m_leaves.tail()});
    }

 //private:

    // structure: tdata< tdata<type_list<...>, ...>,
    //                   tdata<type_list<...>, ...>,
    //                   ...>
    Leaves m_leaves;

};

template<class ArgTypes, class Expr, class Guard, typename... TFuns>
conditional_fun<
    detail::tdata<
        detail::tdata<ArgTypes,
                    typename cfl_<ArgTypes, Expr, Guard, TFuns...>::type>>>
cfun(Expr e, Guard g, TFuns... tfs)
{
    typedef detail::tdata<
                ArgTypes,
                typename cfl_<ArgTypes, Expr, Guard, TFuns...>::type>
        result;
    typedef typename result::tail_type ttype;
    typedef typename ttype::head_type leaf_type;
    typedef typename util::get_callable_trait<Expr>::arg_types expr_args;
    static_assert(ArgTypes::size >= expr_args::size,
                  "Functor has too many arguments");
    return result{
                ArgTypes{},
                leaf_type{std::move(g), std::move(e), std::move(tfs)...}
            };
}

#define VERBOSE(LineOfCode) cout << #LineOfCode << " = " << (LineOfCode) << endl

float tofloat(int i) { return i; }

struct dummy_guard
{
    template<typename... Args>
    inline bool operator()(Args const&...) const
    {
        return true;
    }
};

template<typename... Args>
any_tuple make_any_tuple(Args&&... args)
{
    return make_cow_tuple(std::forward<Args>(args)...);
}

struct from_args { };

template<class Guard, class Transformers, class Pattern>
struct cf_builder
{

    Guard m_guard;
    typename detail::tdata_from_type_list<Transformers>::type m_funs;

 public:

    template<typename... Args>
    cf_builder(from_args const&, Args const&... args) : m_guard(args...), m_funs(args...)
    {
    }

    template<typename G>
    cf_builder(G&& g) : m_guard(std::forward<G>(g))
    {
    }

    template<typename G, typename F>
    cf_builder(G&& g, F&& f) : m_guard(std::forward<G>(g)), m_funs(std::forward<F>(f))
    {
    }

    template<typename NewGuard>
    cf_builder<NewGuard, Transformers, Pattern> when(NewGuard ng)
    {
        return {ng, m_funs};
    }

    template<typename Expr>
    conditional_fun<
        detail::tdata<
            detail::tdata<Pattern,
                          conditional_fun_leaf<Expr, Guard, Transformers, Pattern>>>>
    operator>>(Expr expr) const
    {
        typedef conditional_fun_leaf<Expr, Guard, Transformers, Pattern> tfun;
        typedef detail::tdata<Pattern, tfun> result;
        return result{Pattern{}, tfun{m_guard, std::move(expr), m_funs}};
    }

};

template<typename... T>
cf_builder<dummy_guard, util::type_list<>, util::type_list<T...> > _on()
{
    return {dummy_guard{}};
}

template<bool IsFun, typename T>
struct add_ptr_to_fun_
{
    typedef T* type;
};

template<typename T>
struct add_ptr_to_fun_<false, T>
{
    typedef T type;
};

template<typename T>
struct add_ptr_to_fun : add_ptr_to_fun_<std::is_function<T>::value, T>
{
};

template<bool ToVoid, typename T>
struct to_void_impl
{
    typedef util::void_type type;
};

template<typename T>
struct to_void_impl<false, T>
{
    typedef typename add_ptr_to_fun<T>::type type;
};

template<typename T>
struct not_callable_to_void : to_void_impl<detail::is_boxed<T>::value || !util::is_callable<T>::value, T>
{
};

template<typename T>
struct boxed_and_callable_to_void : to_void_impl<detail::is_boxed<T>::value || util::is_callable<T>::value, T>
{
};

template<typename Pattern>
class value_guard;

template<typename... Types>
class value_guard<util::type_list<Types...> >
{
    detail::tdata<Types...> m_args;

    inline bool _eval(util::void_type const&, detail::tdata<> const&) const
    {
        return true;
    }

    template<class Tail, typename Arg0, typename... Args>
    inline bool _eval(util::void_type const&, Tail const& tail,
                      Arg0 const&, Args const&... args) const
    {
        return _eval(tail.head, tail.tail(), args...);
    }

    template<typename Head, class Tail, typename Arg0, typename... Args>
    inline bool _eval(Head const& head, Tail const& tail,
                      Arg0 const& arg0, Args const&... args) const
    {
        return head == arg0 && _eval(tail.head, tail.tail(), args...);
    }

 public:

    template<typename... Args>
    value_guard(Args const&... args) : m_args(args...) { }

    inline bool operator()(Types const&... args) const
    {
        return _eval(m_args.head, m_args.tail(), args...);
    }
};

template<bool IsCallable, typename T>
struct pattern_type_
{
    typedef util::get_callable_trait<T> ctrait;
    typedef typename ctrait::arg_types args;
    static_assert(args::size == 1, "only unary functions allowed");
    typedef typename util::rm_ref<typename args::head>::type type;
};

template<typename T>
struct pattern_type_<false, T>
{
    typedef typename util::rm_ref<T>::type type;
};

template<typename T>
struct pattern_type : pattern_type_<util::is_callable<T>::value && !detail::is_boxed<T>::value, T>
{
};

template<typename Arg0, typename... Args>
cf_builder<
    value_guard<typename util::tl_apply<util::type_list<Arg0, Args...>, boxed_and_callable_to_void>::type>,
    typename util::tl_apply<util::type_list<Arg0, Args...>, not_callable_to_void>::type,
    util::type_list<typename pattern_type<Arg0>::type,
                    typename pattern_type<Args>::type...> >
_on(Arg0 const& arg0, Args const&... args)
{
    return {from_args{}, arg0, args...};
}

std::string int2str(int i)
{
    return std::to_string(i);
}

option<int> str2int(std::string const& str)
{
    if (str == "42") return 42;
    if (str == "41") return 41;
    return {};
}

typedef util::type_list<int, int, int, float, int, float, float> zz0;

typedef util::type_list<util::type_list<int, int, int>,
                        util::type_list<float>,
                        util::type_list<int>,
                        util::type_list<float, float> > zz8;

typedef util::type_list<
            util::type_list<
                util::type_pair<std::integral_constant<size_t,0>, int>,
                util::type_pair<std::integral_constant<size_t,1>, int>,
                util::type_pair<std::integral_constant<size_t,2>, int>
            >,
            util::type_list<
                util::type_pair<std::integral_constant<size_t,3>, float>
            >,
            util::type_list<
                util::type_pair<std::integral_constant<size_t,4>, int>
            >,
            util::type_list<
                util::type_pair<std::integral_constant<size_t,5>, float>,
                util::type_pair<std::integral_constant<size_t,6>, float>
            >
        >
        zz9;


template<typename First, typename Second>
struct is_same_ : std::is_same<typename First::second, typename Second::second>
{
};

size_t test__tuple()
{
    CPPA_TEST(test__tuple);

    using namespace cppa::placeholders;

    typedef typename util::tl_group_by<zz0, std::is_same>::type zz1;

    typedef typename util::tl_zip_with_index<zz0>::type zz2;

    static_assert(std::is_same<zz1, zz8>::value, "group_by failed");

    typedef typename util::tl_group_by<zz2, is_same_>::type zz3;

    static_assert(std::is_same<zz3, zz9>::value, "group_by failed");

    cout << detail::demangle(typeid(zz3).name()) << endl;

    exit(0);

    typedef util::type_list<int, int> token1;
    typedef util::type_list<float> token2;

    //auto f00 = cfun<token1>([](int, int) { cout << "f0[1]!" << endl; }, _x1 > _x2)
    //          .or_else(cfun<token1>([](int, int) { cout << "f0[2]!" << endl; }, _x1 == 2 && _x2 == 2))
    //          .or_else(cfun<token2>([](float) { cout << "f0[3]!" << endl; }, dummy_guard{}));

    auto f00 = _on<int, int>() >> []() { };
    auto f01 = _on<int, int>().when(_x1 == 42) >> []() { };
    auto f02 = _on<int, int>().when(_x1 == 42 && _x2 * 2 == _x1) >> []() { };

    auto f03 = _on(42, val<int>) >> [](int) { };

    auto f04 = _on(42, int2str).when(_x2 == "42") >> []() { };
    auto f05 = _on(42, str2int).when(_x2 % 2 == 0) >> []() { };

    VERBOSE(f00(42, 42));
    VERBOSE(f01(42, 42));
    VERBOSE(f02(42, 42));
    VERBOSE(f02(42, 21));
    VERBOSE(f03(42, 42));

    VERBOSE(f04(42, 42));
    VERBOSE(f04(42, std::string("42")));

    VERBOSE(f05(42, 42));
    VERBOSE(f05(42, std::string("41")));
    VERBOSE(f05(42, std::string("42")));
    VERBOSE(f05(42, std::string("hello world!")));

    exit(0);

    auto f0 = cfun<token1>([](int, int) { cout << "f0[0]!" << endl; }, _x1 < _x2)
             //.or_else(f00)
             .or_else(cfun<token1>([](int, int) { cout << "f0[1]!" << endl; }, _x1 > _x2))
             .or_else(cfun<token1>([](int, int) { cout << "f0[2]!" << endl; }, _x1 == 2 && _x2 == 2))
             .or_else(cfun<token2>([](float) { cout << "f0[3]!" << endl; }, dummy_guard{}))
             .or_else(cfun<token1>([](int, int) { cout << "f0[4]" << endl; }, dummy_guard{}));

    //VERBOSE(f0(make_cow_tuple(1, 2)));

    VERBOSE(f0(3, 3));
    VERBOSE(f0.invoke(make_any_tuple(3, 3)));

    VERBOSE(f0.invoke(make_any_tuple(2, 2)));
    VERBOSE(f0.invoke(make_any_tuple(3, 2)));
    VERBOSE(f0.invoke(make_any_tuple(1.f)));

    auto f1 = cfun<token1>([](float, int) { cout << "f1!" << endl; }, _x1 < 6, tofloat);

    VERBOSE(f1.invoke(make_any_tuple(5, 6)));
    VERBOSE(f1.invoke(make_any_tuple(6, 7)));

    auto i2 = make_any_tuple(1, 2);

    VERBOSE(f0.invoke(*i2.vals()->type_token(), i2.vals()->impl_type(), i2.vals()->native_data(), *(i2.vals())));
    VERBOSE(f1.invoke(*i2.vals()->type_token(), i2.vals()->impl_type(), i2.vals()->native_data(), *(i2.vals())));

    any_tuple dt1;
    {
        auto oarr = new detail::object_array;
        oarr->push_back(object::from(1));
        oarr->push_back(object::from(2));
        dt1 = any_tuple{static_cast<detail::abstract_tuple*>(oarr)};
    }


    VERBOSE(f0.invoke(*dt1.cvals()->type_token(), dt1.cvals()->impl_type(), dt1.cvals()->native_data(), *dt1.cvals()));
    VERBOSE(f1.invoke(*dt1.cvals()->type_token(), dt1.cvals()->impl_type(), dt1.cvals()->native_data(), *dt1.cvals()));

    exit(0);


    // check type correctness of make_cow_tuple()
    auto t0 = make_cow_tuple("1", 2);
    CPPA_CHECK((std::is_same<decltype(t0), cppa::cow_tuple<std::string, int>>::value));
    auto t0_0 = get<0>(t0);
    auto t0_1 = get<1>(t0);
    // check implicit type conversion
    CPPA_CHECK((std::is_same<decltype(t0_0), std::string>::value));
    CPPA_CHECK((std::is_same<decltype(t0_1), int>::value));
    CPPA_CHECK_EQUAL(t0_0, "1");
    CPPA_CHECK_EQUAL(t0_1, 2);
    // use tuple cast to get a subtuple
    any_tuple at0(t0);
    auto v0opt = tuple_cast<std::string, anything>(at0);
    CPPA_CHECK((std::is_same<decltype(v0opt), option<cow_tuple<std::string>>>::value));
    CPPA_CHECK((v0opt));
    CPPA_CHECK(   at0.size() == 2
               && at0.at(0) == &get<0>(t0)
               && at0.at(1) == &get<1>(t0));
    if (v0opt)
    {
        auto& v0 = *v0opt;
        CPPA_CHECK((std::is_same<decltype(v0), cow_tuple<std::string>&>::value));
        CPPA_CHECK((std::is_same<decltype(get<0>(v0)), std::string const&>::value));
        CPPA_CHECK_EQUAL(v0.size(), 1);
        CPPA_CHECK_EQUAL(get<0>(v0), "1");
        CPPA_CHECK_EQUAL(get<0>(t0), get<0>(v0));
        // check cow semantics
        CPPA_CHECK_EQUAL(&get<0>(t0), &get<0>(v0));     // point to the same
        get_ref<0>(t0) = "hello world";                 // detaches t0 from v0
        CPPA_CHECK_EQUAL(get<0>(t0), "hello world");    // t0 contains new value
        CPPA_CHECK_EQUAL(get<0>(v0), "1");              // v0 contains old value
        CPPA_CHECK_NOT_EQUAL(&get<0>(t0), &get<0>(v0)); // no longer the same
        // check operator==
        auto lhs = make_cow_tuple(1,2,3,4);
        auto rhs = make_cow_tuple(static_cast<std::uint8_t>(1), 2.0, 3, 4);
        CPPA_CHECK(lhs == rhs);
        CPPA_CHECK(rhs == lhs);
    }
    any_tuple at1 = make_cow_tuple("one", 2, 3.f, 4.0);
    {
        // perfect match
        auto opt0 = tuple_cast<std::string, int, float, double>(at1);
        CPPA_CHECK(opt0);
        if (opt0)
        {
            CPPA_CHECK((*opt0 == make_cow_tuple("one", 2, 3.f, 4.0)));
            CPPA_CHECK_EQUAL(&get<0>(*opt0), at1.at(0));
            CPPA_CHECK_EQUAL(&get<1>(*opt0), at1.at(1));
            CPPA_CHECK_EQUAL(&get<2>(*opt0), at1.at(2));
            CPPA_CHECK_EQUAL(&get<3>(*opt0), at1.at(3));
        }
        // leading wildcard
        auto opt1 = tuple_cast<anything, double>(at1);
        CPPA_CHECK(opt1);
        if (opt1)
        {
            CPPA_CHECK_EQUAL(get<0>(*opt1), 4.0);
            CPPA_CHECK_EQUAL(&get<0>(*opt1), at1.at(3));
        }
        // trailing wildcard
        auto opt2 = tuple_cast<std::string, anything>(at1);
        CPPA_CHECK(opt2);
        if (opt2)
        {
            CPPA_CHECK_EQUAL(get<0>(*opt2), "one");
            CPPA_CHECK_EQUAL(&get<0>(*opt2), at1.at(0));
        }
        // wildcard in between
        auto opt3 = tuple_cast<std::string, anything, double>(at1);
        CPPA_CHECK(opt3);
        if (opt3)
        {
            CPPA_CHECK((*opt3 == make_cow_tuple("one", 4.0)));
            CPPA_CHECK_EQUAL(get<0>(*opt3), "one");
            CPPA_CHECK_EQUAL(get<1>(*opt3), 4.0);
            CPPA_CHECK_EQUAL(&get<0>(*opt3), at1.at(0));
            CPPA_CHECK_EQUAL(&get<1>(*opt3), at1.at(3));
        }
    }
    return CPPA_TEST_RESULT;
}
