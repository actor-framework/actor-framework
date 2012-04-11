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
#include "cppa/tpartial_function.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/rm_option.hpp"

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/types_array.hpp"
#include "cppa/detail/object_array.hpp"

#include <boost/progress.hpp>

using std::cout;
using std::endl;

using namespace cppa;

template<typename AbstractTuple>
struct invoke_policy_helper
{
    size_t i;
    AbstractTuple& tup;
    invoke_policy_helper(AbstractTuple& tp) : i(0), tup(tp) { }
    template<typename T>
    void operator()(ge_mutable_reference_wrapper<T>& storage)
    {
        storage = *reinterpret_cast<T*>(tup.mutable_at(i++));
    }
    template<typename T>
    void operator()(ge_mutable_reference_wrapper<const T>& storage)
    {
        storage = *reinterpret_cast<T const*>(tup.at(i++));
    }
};

template<typename T>
struct gref_wrapped
{
    typedef ge_reference_wrapper<typename util::rm_ref<T>::type> type;
};

template<typename T>
struct gref_mutable_wrapped
{
    typedef ge_mutable_reference_wrapper<T> type;
};

template<typename T>
struct gref_mutable_wrapped<T&>
{
    typedef ge_mutable_reference_wrapper<T> type;
};

template<typename T>
struct rm_all_refs_
{
    typedef T type;
};

template<typename T>
struct rm_all_refs_<ge_reference_wrapper<T> >
{
    typedef T type;
};

template<typename T>
struct rm_all_refs_<ge_mutable_reference_wrapper<T> >
{
    typedef T type;
};

template<typename T>
struct rm_all_refs_<std::reference_wrapper<T> >
{
    typedef T type;
};


template<typename T>
struct rm_all_refs
{
    typedef typename rm_all_refs_<typename util::rm_ref<T>::type>::type type;
};

template<wildcard_position, class Pattern, class FilteredPattern>
struct invoke_policy_impl;

template<class Pattern, class FilteredPattern>
struct invoke_policy_impl<wildcard_position::nil, Pattern, FilteredPattern>
{
    typedef typename detail::tdata_from_type_list<
                FilteredPattern
            >::type
            native_data_type;

    typedef typename detail::static_types_array_from_type_list<
                FilteredPattern
            >::type
            arr_type;

    template<class Target, class Tup>
    static inline bool shortcut(Target&, Tup&, bool&)
    {
        return false;
    }

    template<class Target, typename... T>
    static inline bool shortcut(Target& target,
                                detail::tdata<T...> const& tup,
                                bool& shortcut_result,
                                typename util::enable_if<
                                    util::tl_binary_forall<
                                        util::type_list<
                                            typename rm_all_refs<T>::type...
                                        >,
                                        FilteredPattern,
                                        std::is_same
                                    >
                                >::type* = 0)
    {
        shortcut_result = util::unchecked_apply_tuple<bool>(target, tup);
        return true;
    }

    template<class Target, typename... T>
    static inline bool shortcut(Target& target,
                                detail::tdata<T...>& tup,
                                bool& shortcut_result,
                                typename util::enable_if<
                                    util::tl_binary_forall<
                                        util::type_list<
                                            typename rm_all_refs<T>::type...
                                        >,
                                        FilteredPattern,
                                        std::is_same
                                    >
                                >::type* = 0)
    {
        //static_assert(false, "");
        shortcut_result = util::unchecked_apply_tuple<bool>(target, tup);
        return true;
    }

    template<class Target, typename NativeArg, typename Tuple>
    static bool invoke(Target& target,
                       std::type_info const& arg_types,
                       detail::tuple_impl_info timpl,
                       NativeArg* native_arg,
                       Tuple& tup)
    {
        bool shortcut_result = false;
        if (shortcut(target, tup, shortcut_result))
        {
            return shortcut_result;
        }
        else if (arg_types == typeid(FilteredPattern))
        {
            if (native_arg)
            {
                typedef typename util::if_else_c<
                            std::is_const<NativeArg>::value,
                            native_data_type const*,
                            util::wrapped<native_data_type*>
                        >::type
                        cast_type;
                auto arg = reinterpret_cast<cast_type>(native_arg);
                return util::unchecked_apply_tuple<bool>(target, *arg);
            }
            // 'fall through'
        }
        else if (timpl == detail::dynamically_typed)
        {
            auto& arr = arr_type::arr;
            if (tup.size() != FilteredPattern::size)
            {
                return false;
            }
            for (size_t i = 0; i < FilteredPattern::size; ++i)
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
        typedef typename detail::tdata_from_type_list<
                    typename util::tl_map<
                        typename util::if_else<
                            std::is_const<Tuple>,
                            typename util::tl_map<Pattern, std::add_const>::type,
                            util::wrapped<Pattern>
                        >::type,
                        gref_mutable_wrapped
                    >::type
                >::type
                ttup_type;

        ttup_type ttup;

        typedef typename util::if_else<
                    std::is_const<Tuple>,
                    ttup_type const&,
                    util::wrapped<ttup_type&>
                >::type
                ttup_ref;

        //typename PolicyToken::wrapped_refs ttup;
        invoke_policy_helper<Tuple> helper{tup};
        util::static_foreach<0, FilteredPattern::size>::_ref(ttup, helper);
        //return util::apply_tuple(target, ttup);
        ttup_ref ttup_fwd = ttup;
        return util::unchecked_apply_tuple<bool>(target, ttup_fwd);
    }
};

template<>
struct invoke_policy_impl<wildcard_position::leading,
                          util::type_list<anything>,
                          util::type_list<> >
{
    template<class Target, typename NativeArg, class Tuple>
    static bool invoke(Target& target,
                       std::type_info const&,
                       detail::tuple_impl_info,
                       NativeArg*,
                       Tuple&)
    {
        return target();
    }
};

template<class Pattern, class FilteredPattern>
struct invoke_policy_impl<wildcard_position::trailing, Pattern, FilteredPattern>
{
    template<class Target, typename NativeArg, class Tuple>
    static bool invoke(Target& target,
                       std::type_info const&,
                       detail::tuple_impl_info,
                       NativeArg*,
                       Tuple& tup)
    {
        typedef typename detail::static_types_array_from_type_list<
                    FilteredPattern
                >::type
                arr_type;
        auto& arr = arr_type::arr;
        if (tup.size() < FilteredPattern::size)
        {
            return false;
        }
        for (size_t i = 0; i < FilteredPattern::size; ++i)
        {
            if (arr[i] != tup.type_at(i))
            {
                return false;
            }
        }
        typedef typename detail::tdata_from_type_list<
                    typename util::tl_map<
                        typename util::if_else<
                            std::is_const<Tuple>,
                            typename util::tl_map<FilteredPattern, std::add_const>::type,
                            util::wrapped<FilteredPattern>
                        >::type,
                        gref_mutable_wrapped
                    >::type
                >::type
                ttup_type;

        ttup_type ttup;

        typedef typename util::if_else<
                    std::is_const<Tuple>,
                    ttup_type const&,
                    util::wrapped<ttup_type&>
                >::type
                ttup_ref;

        //typename PolicyToken::wrapped_refs ttup;
        invoke_policy_helper<Tuple> helper{tup};
        util::static_foreach<0, FilteredPattern::size>::_ref(ttup, helper);
        //return util::apply_tuple(target, ttup);
        ttup_ref ttup_fwd = ttup;
        return util::unchecked_apply_tuple<bool>(target, ttup_fwd);
    }

};

template<class Pattern>
struct invoke_policy
        : invoke_policy_impl<
            get_wildcard_position<Pattern>(),
            Pattern,
            typename util::tl_filter_not_type<Pattern, anything>::type>
{
};

template<class PartialFun>
struct projection_helper
{
    PartialFun const& fun;
    projection_helper(PartialFun const& pfun) : fun(pfun) { }
    template<typename... Args>
    bool operator()(Args&&... args) const
    {
        if (fun.defined_at(std::forward<Args>(args)...))
        {
            fun(std::forward<Args>(args)...);
            return true;
        }
        return false;
    }
};

template<typename T>
struct add_const_ref
{
    typedef T const& type;
};

template<typename T>
struct add_ref_if_not_void
{
    typedef T& type;
};

template<>
struct add_ref_if_not_void<util::void_type>
{
    typedef util::void_type type;
};

template<typename T>
struct deduce_result
{
    typedef typename util::rm_option<typename util::get_result_type<T>::type>::type type;
};

template<>
struct deduce_result<util::void_type>
{
    typedef util::void_type type;
};

template<typename T>
struct deduce_unary_arg
{
    typedef typename util::get_arg_types<T>::types arg_types;
    static_assert(arg_types::size == 1, "not a unary function");
    typedef typename arg_types::head type;
};

template<>
struct deduce_unary_arg<util::void_type>
{
    typedef util::void_type type;
};

template<typename T0, typename T1>
struct deduce_ref_type
{
    typedef T1 type;
};

template<typename T>
struct deduce_ref_type<T&, T>
{
    typedef T& type;
};

/**
 * @brief Projection implemented by a set of functors.
 */
template<class Pattern, class TargetSignature, class ProjectionFuns>
class projection
{

    typedef typename detail::tdata_from_type_list<ProjectionFuns>::type
            fun_container;

    fun_container m_funs;

 public:

    typedef Pattern pattern_type;

    typedef typename util::tl_filter_not_type<
                pattern_type,
                anything
            >::type
            filtered_pattern;

    static_assert(ProjectionFuns::size <= filtered_pattern::size,
                  "invalid projection (too many functions)");

    typedef typename util::tl_pad_left<
                TargetSignature,
                filtered_pattern::size
            >::type
            padded_signature;

    typedef typename util::tl_pad_left<
                ProjectionFuns,
                filtered_pattern::size
            >::type
            padded_projection_funs;

    typedef typename util::tl_map<
                padded_projection_funs,
                deduce_result
            >::type
            padded_result_types;

    typedef typename util::tl_zip<
                typename util::tl_zip<
                    typename util::tl_map<
                        padded_result_types,
                        add_ref_if_not_void
                    >::type,
                    padded_signature,
                    util::left_or_right
                >::type,
                typename util::tl_map<
                    filtered_pattern,
                    add_const_ref
                >::type,
                util::left_or_right
            >::type
            projected_arg_types;

    projection(fun_container const& args) : m_funs(args)
    {
    }

    projection(projection const&) = default;

    /**
     * @brief Invokes @p fun with a projection of <tt>args...</tt>.
     */
    template<class PartialFun, typename... Args>
    bool operator()(PartialFun& fun, Args&&... args) const
    {
        // can collect if ...
        static constexpr bool can_collect =
                // ... for each arg ...
                util::tl_binary_forall<
                    util::type_list<Args...>,
                    typename util::tl_zip<
                        // ... args match conversion functions ...
                        typename util::tl_map<
                            padded_projection_funs,
                            deduce_unary_arg
                        >::type,
                        // ... and reference types match
                        typename util::tl_zip<
                            padded_signature,
                            filtered_pattern,
                            deduce_ref_type
                        >::type,
                        util::left_or_right
                    >::type,
                    std::is_convertible
                >::value;

        std::integral_constant<bool, can_collect> token;
        return invoke(token, fun, std::forward<Args>(args)...);
    }

 private:

    template<class PartialFun, typename... Args>
    bool invoke(std::integral_constant<bool, false>,
                PartialFun&, Args&&...              ) const
    {
        return false;
    }

    template<class PartialFun, typename... Args>
    bool invoke(std::integral_constant<bool, true>,
                PartialFun& fun, Args&&... args    ) const
    {
        typedef typename util::tl_zip<
                    padded_result_types,
                    typename util::tl_map<
                        projected_arg_types,
                        gref_mutable_wrapped
                    >::type,
                    util::left_or_right
                >::type
                collected_arg_types;

        typename detail::tdata_from_type_list<collected_arg_types>::type pargs;
        if (collect(pargs, m_funs, std::forward<Args>(args)...))
        {
            projection_helper<PartialFun> helper{fun};
            return util::unchecked_apply_tuple<bool>(helper, pargs);
        }
        return false;
    }

    template<typename Storage, typename T>
    static inline  bool fetch_(Storage& storage, T&& value)
    {
        storage = std::forward<T>(value);
        return true;
    }

    template<class Storage>
    static inline bool fetch_(Storage& storage, option<Storage>&& value)
    {
        if (value)
        {
            storage = std::move(*value);
            return true;
        }
        return false;
    }

    template<class Storage, typename Fun, typename T>
    static inline  bool fetch(Storage& storage, Fun const& fun, T&& arg)
    {
        return fetch_(storage, fun(std::forward<T>(arg)));
    }

    template<typename Storage, typename T>
    static inline bool fetch(Storage& storage, util::void_type const&, T&& arg)
    {
        return fetch_(storage, std::forward<T>(arg));
    }

    static inline bool collect(detail::tdata<>&, detail::tdata<> const&)
    {
        return true;
    }

    template<class TData, class Trans, typename T0, typename... Ts>
    static inline bool collect(TData& td, Trans const& tr,
                               T0&& arg0, Ts&&... args)
    {
        return    fetch(td.head, tr.head, std::forward<T0>(arg0))
               && collect(td.tail(), tr.tail(), std::forward<Ts>(args)...);
    }

};

template<>
class projection<util::type_list<anything>, util::type_list<>, util::type_list<> >
{

 public:

    template<typename... Args>
    projection(Args const&...) { }

    typedef util::type_list<anything> pattern_type;
    typedef util::type_list<> projected_arg_types;

    template<class PartialFun>
    bool operator()(PartialFun& fun) const
    {
        fun();
        return true;
    }
};

template<class Expr, class Guard, class Transformers, class Pattern>
struct get_cfl
{
    typedef typename util::get_callable_trait<Expr>::type ctrait;
    typedef typename util::tl_filter_not_type<Pattern, anything>::type argl;
    typedef projection<Pattern, typename ctrait::arg_types, Transformers> type1;
    typedef typename get_tpartial_function<
                Expr,
                Guard,
                typename type1::projected_arg_types
            >::type
            type2;
    typedef std::pair<type1, type2> type;
};

template<typename First, typename Second>
struct pjf_same_pattern
        : std::is_same<typename First::second::first_type::pattern_type,
                       typename Second::second::first_type::pattern_type>
{
};

// last invocation step; evaluates a {projection, tpartial_function} pair
template<typename Data>
struct invoke_helper3
{
    Data const& data;
    invoke_helper3(Data const& mdata) : data(mdata) { }
    template<size_t Pos, typename T, typename... Args>
    inline bool operator()(util::type_pair<std::integral_constant<size_t, Pos>, T>,
                           Args&&... args) const
    {
        auto const& target = get<Pos>(data);
        return target.first(target.second, std::forward<Args>(args)...);
        //return (get<Pos>(data))(args...);
    }
};

template<class Data, class Token, class Pattern>
struct invoke_helper2
{
    typedef Pattern pattern_type;
    typedef typename util::tl_filter_not_type<pattern_type, anything>::type arg_types;
    Data const& data;
    invoke_helper2(Data const& mdata) : data(mdata) { }
    template<typename... Args>
    bool invoke(Args&&... args) const
    {
        typedef invoke_policy<Pattern> impl;
        return impl::invoke(*this, std::forward<Args>(args)...);
    }
    // resolved argument list (called from invoke_policy)
    template<typename... Args>
    bool operator()(Args&&... args) const
    {
        //static_assert(false, "foo");
        Token token;
        invoke_helper3<Data> fun{data};
        return util::static_foreach<0, Token::size>::eval_or(token, fun, std::forward<Args>(args)...);
    }
};

// invokes a group of {projection, tpartial_function} pairs
template<typename Data>
struct invoke_helper
{
    Data const& data;
    invoke_helper(Data const& mdata) : data(mdata) { }
    // token: type_list<type_pair<integral_constant<size_t, X>,
    //                            std::pair<projection, tpartial_function>>,
    //                  ...>
    // all {projection, tpartial_function} pairs have the same pattern
    // thus, can be invoked from same data
    template<class Token, typename... Args>
    bool operator()(Token, Args&&... args) const
    {
        typedef typename Token::head type_pair;
        typedef typename type_pair::second leaf_pair;
        typedef typename leaf_pair::first_type projection_type;
        // next invocation step
        invoke_helper2<Data,
                       Token,
                       typename projection_type::pattern_type> fun{data};
        return fun.invoke(std::forward<Args>(args)...);
    }
};

template<typename T>
struct is_manipulator_leaf;

template<typename First, typename Second>
struct is_manipulator_leaf<std::pair<First, Second> >
{
    static constexpr bool value = Second::manipulates_args;
};

void collect_tdata(detail::tdata<>&)
{
}

template<typename Storage, typename... Args>
void collect_tdata(Storage& storage, detail::tdata<> const&, Args const&... args)
{
    collect_tdata(storage, args...);
}

template<typename Storage, typename Arg0, typename... Args>
void collect_tdata(Storage& storage, Arg0 const& arg0, Args const&... args)
{
    storage = arg0.head;
    collect_tdata(storage.tail(), arg0.tail(), args...);
}

template<bool IsManipulator, typename T0, typename T1>
struct pj_fwd_
{
    typedef T1 type;
};

template<typename T>
struct pj_fwd_<false, T const&, T>
{
    typedef std::reference_wrapper<const T> type;
};

template<typename T>
struct pj_fwd_<true, T&, T>
{
    typedef std::reference_wrapper<T> type;
};

template<bool IsManipulator, typename T>
struct pj_fwd
{
    typedef typename pj_fwd_<
                IsManipulator,
                T,
                typename detail::implicit_conversions<
                    typename util::rm_ref<T>::type
                >::type
            >::type
            type;
};

/**
 * @brief A function that works on the projection of given data rather than
 *        on the data itself.
 */
template<class... Leaves>
class projected_fun
{

 public:

    typedef util::type_list<Leaves...> leaves_list;
    typedef typename util::tl_zip_with_index<leaves_list>::type zipped_list;
    typedef typename util::tl_group_by<zipped_list, pjf_same_pattern>::type
            eval_order;

    static constexpr bool has_manipulator =
            util::tl_exists<leaves_list, is_manipulator_leaf>::value;

    template<typename... Args>
    projected_fun(Args&&... args) : m_leaves(std::forward<Args>(args)...)
    {
    }

    projected_fun(projected_fun&& other) : m_leaves(std::move(other.m_leaves))
    {
    }

    projected_fun(projected_fun const&) = default;

    bool _invoke(any_tuple const& tup) const
    {
        eval_order token;
        invoke_helper<decltype(m_leaves)> fun{m_leaves};
        auto& cvals = *(tup.cvals());
        return util::static_foreach<0, eval_order::size>
               ::eval_or(token,
                         fun,
                         *(cvals.type_token()),
                         cvals.impl_type(),
                         cvals.native_data(),
                         cvals);
    }

    bool _invoke(any_tuple& tup) const
    {
        eval_order token;
        invoke_helper<decltype(m_leaves)> fun{m_leaves};
        tup.force_detach();
        auto& vals = *(tup.vals());
        return util::static_foreach<0, eval_order::size>
               ::eval_or(token,
                         fun,
                         *(vals.type_token()),
                         vals.impl_type(),
                         vals.mutable_native_data(),
                         vals);
    }

    bool invoke(any_tuple const& tup) const
    {
        if (has_manipulator)
            return invoke(any_tuple{tup});
        return _invoke(tup);
    }

    bool invoke(any_tuple& tup) const
    {
        if (!has_manipulator)
            return _invoke(static_cast<any_tuple const&>(tup));
        return _invoke(tup);
    }

    bool invoke(any_tuple&& tup) const
    {
        any_tuple tmp{tup};
        return invoke(tmp);
    }

    template<typename... Args>
    bool operator()(Args&&... args) const
    {
        typedef detail::tdata<typename pj_fwd<has_manipulator, Args>::type...>
                tuple_type;
        // applies implicit conversions etc
        tuple_type tup{std::forward<Args>(args)...};

        typedef typename util::if_else_c<
                    has_manipulator,
                    tuple_type&,
                    util::wrapped<tuple_type const&>
                >::type
                ref_type;

        typedef typename util::if_else_c<
                    has_manipulator,
                    void*,
                    util::wrapped<void const*>
                >::type
                ptr_type;

        eval_order token;
        invoke_helper<decltype(m_leaves)> fun{m_leaves};
        return util::static_foreach<0, eval_order::size>
                ::eval_or(token,
                          fun,
                          typeid(util::type_list<typename util::rm_ref<Args>::type...>),
                          detail::statically_typed,
                          static_cast<ptr_type>(nullptr),
                          static_cast<ref_type>(tup));
    }

    template<class... Rhs>
    projected_fun<Leaves..., Rhs...>
    or_else(projected_fun<Rhs...> const& other) const
    {
        detail::tdata<ge_reference_wrapper<Leaves>...,
                      ge_reference_wrapper<Rhs>...    > all_leaves;
        collect_tdata(all_leaves, m_leaves, other.m_leaves);
        return {all_leaves};
    }

 //private:

    // structure: tdata< tdata<type_list<...>, ...>,
    //                   tdata<type_list<...>, ...>,
    //                   ...>
    detail::tdata<Leaves...> m_leaves;

};

template<class List>
struct projected_fun_from_type_list;

template<typename... Args>
struct projected_fun_from_type_list<util::type_list<Args...> >
{
    typedef projected_fun<Args...> type;
};

template<typename... Lhs, typename... Rhs>
projected_fun<Lhs..., Rhs...> operator,(projected_fun<Lhs...> const& lhs,
                                        projected_fun<Rhs...> const& rhs)
{
    return lhs.or_else(rhs);
}

template<typename Arg0, typename... Args>
typename projected_fun_from_type_list<
    typename util::tl_concat<
        typename Arg0::leaves_list,
        typename Args::leaves_list...
    >::type
>::type
pj_concat(Arg0 const& arg0, Args const&... args)
{
    typename detail::tdata_from_type_list<
        typename util::tl_map<
            typename util::tl_concat<
                typename Arg0::leaves_list,
                typename Args::leaves_list...
            >::type,
            gref_wrapped
        >::type
    >::type
    all_leaves;
    collect_tdata(all_leaves, arg0.m_leaves, args.m_leaves...);
    return {all_leaves};
}

#define VERBOSE(LineOfCode) cout << #LineOfCode << " = " << (LineOfCode) << endl

template<typename... Args>
any_tuple make_any_tuple(Args&&... args)
{
    return make_cow_tuple(std::forward<Args>(args)...);
}

template<bool IsFun, typename T>
struct vg_fwd_
{
    static inline T const& _(T const& arg) { return arg; }
    static inline T&& _(T&& arg) { return std::move(arg); }
    static inline T& _(T& arg) { return arg; }
};

template<typename T>
struct vg_fwd_<true, T>
{
    template<typename Arg>
    static inline util::void_type _(Arg&&) { return {}; }
};

// absorbs functors
template<typename T>
struct vg_fwd
        : vg_fwd_<util::is_callable<typename util::rm_ref<T>::type>::value,
                  typename util::rm_ref<T>::type>
{
};

template<typename FilteredPattern>
class value_guard
{
    typename detail::tdata_from_type_list<FilteredPattern>::type m_args;

    template<typename... Args>
    inline bool _eval(util::void_type const&,
                      detail::tdata<> const&, Args&&...) const
    {
        return true;
    }

    template<class Tail, typename Arg0, typename... Args>
    inline bool _eval(util::void_type const&, Tail const& tail,
                      Arg0 const&, Args const&... args         ) const
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

    value_guard() = default;
    value_guard(value_guard const&) = default;

    template<typename... Args>
    value_guard(Args const&... args) : m_args(vg_fwd<Args>::_(args)...)
    {
    }

    template<typename... Args>
    inline bool operator()(Args const&... args) const
    {
        return _eval(m_args.head, m_args.tail(), args...);
    }
};

typedef value_guard< util::type_list<> > dummy_guard;

struct cf_builder_from_args { };

template<class Guard, class Transformers, class Pattern>
struct cf_builder
{

    typedef typename detail::tdata_from_type_list<Transformers>::type
            fun_container;

    Guard m_guard;
    typename detail::tdata_from_type_list<Transformers>::type m_funs;

 public:

    cf_builder() = default;

    template<typename... Args>
    cf_builder(cf_builder_from_args const&, Args const&... args)
        : m_guard(args...)
        , m_funs(args...)
    {
    }

    cf_builder(Guard& mg, fun_container const& funs)
        : m_guard(std::move(mg)), m_funs(funs)
    {
    }

    cf_builder(Guard const& mg, fun_container const& funs)
        : m_guard(mg), m_funs(funs)
    {
    }

    template<typename NewGuard>
    cf_builder<
        guard_expr<
            logical_and_op,
            guard_expr<exec_xfun_op, Guard, util::void_type>,
            NewGuard>,
        Transformers,
        Pattern>
    when(NewGuard ng,
         typename util::disable_if_c<
               std::is_same<NewGuard, NewGuard>::value
            && std::is_same<Guard, dummy_guard>::value
         >::type* = 0                                 ) const
    {
        return {(gcall(m_guard) && ng), m_funs};
    }

    template<typename NewGuard>
    cf_builder<NewGuard, Transformers, Pattern>
    when(NewGuard ng,
         typename util::enable_if_c<
               std::is_same<NewGuard, NewGuard>::value
            && std::is_same<Guard, dummy_guard>::value
         >::type* = 0                                 ) const
    {
        return {ng, m_funs};
    }

    template<typename Expr>
    projected_fun<typename get_cfl<Expr, Guard, Transformers, Pattern>::type>
    operator>>(Expr expr) const
    {
        typedef typename get_cfl<Expr, Guard, Transformers, Pattern>::type tpair;
        return tpair{typename tpair::first_type{m_funs},
                     typename tpair::second_type{std::move(expr), m_guard}};
    }

};

template<typename... T>
cf_builder<value_guard<util::type_list<> >,
           util::type_list<>,
           util::type_list<T...> > _on()
{
    return {};
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
    typedef typename util::rm_ref<typename detail::unboxed<T>::type>::type type;
};

template<typename T>
struct pattern_type : pattern_type_<util::is_callable<T>::value && !detail::is_boxed<T>::value, T>
{
};

template<typename Arg0, typename... Args>
cf_builder<
    value_guard<
        typename util::tl_trim<
            typename util::tl_map<
                util::type_list<Arg0, Args...>,
                boxed_and_callable_to_void
            >::type,
            util::void_type
        >::type
    >,
    typename util::tl_map<
        util::type_list<Arg0, Args...>,
        not_callable_to_void
    >::type,
    util::type_list<typename pattern_type<Arg0>::type,
                    typename pattern_type<Args>::type...> >
_on(Arg0 const& arg0, Args const&... args)
{
    return {cf_builder_from_args{}, arg0, args...};
}

std::string int2str(int i)
{
    return std::to_string(i);
}

option<int> str2int(std::string const& str)
{
    char* endptr = nullptr;
    int result = static_cast<int>(strtol(str.c_str(), &endptr, 10));
    if (endptr != nullptr && *endptr == '\0')
    {
        return result;
    }
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

#define CPPA_CHECK_INVOKED(FunName, Args)                                      \
    if ( ( FunName Args ) == false || invoked != #FunName ) {                  \
        CPPA_ERROR("invocation of " #FunName " failed");                       \
    } invoked = ""

#define CPPA_CHECK_NOT_INVOKED(FunName, Args)                                  \
    if ( ( FunName Args ) == true || invoked == #FunName ) {                   \
        CPPA_ERROR(#FunName " erroneously invoked");                           \
    } invoked = ""

size_t test__tuple()
{
    CPPA_TEST(test__tuple);

    using namespace cppa::placeholders;

    typedef typename util::tl_group_by<zz0, std::is_same>::type zz1;

    typedef typename util::tl_zip_with_index<zz0>::type zz2;

    static_assert(std::is_same<zz1, zz8>::value, "group_by failed");

    typedef typename util::tl_group_by<zz2, is_same_>::type zz3;

    static_assert(std::is_same<zz3, zz9>::value, "group_by failed");

    typedef util::type_list<int, int> token1;
    typedef util::type_list<float> token2;

    std::string invoked;

    auto f00 = _on<int, int>() >> [&]() { invoked = "f00"; };
    CPPA_CHECK_INVOKED(f00, (42, 42));

    auto f01 = _on<int, int>().when(_x1 == 42) >> [&]() { invoked = "f01"; };
    CPPA_CHECK_INVOKED(f01, (42, 42));
    CPPA_CHECK_NOT_INVOKED(f01, (1, 2));

    auto f02 = _on<int, int>().when(_x1 == 42 && _x2 * 2 == _x1) >> [&]() { invoked = "f02"; };
    CPPA_CHECK_NOT_INVOKED(f02, (0, 0));
    CPPA_CHECK_NOT_INVOKED(f02, (42, 42));
    CPPA_CHECK_NOT_INVOKED(f02, (2, 1));
    CPPA_CHECK_INVOKED(f02, (42, 21));

    CPPA_CHECK(f02.invoke(make_any_tuple(42, 21)));
    CPPA_CHECK_EQUAL("f02", invoked);
    invoked = "";

    auto f03 = _on(42, val<int>) >> [&](int const& a, int&) { invoked = "f03"; CPPA_CHECK_EQUAL(42, a); };
    CPPA_CHECK_NOT_INVOKED(f03, (0, 0));
    CPPA_CHECK_INVOKED(f03, (42, 42));

    auto f04 = _on(42, int2str).when(_x2 == "42") >> [&](std::string& str)
    {
        CPPA_CHECK_EQUAL("42", str);
        invoked = "f04";
    };

    CPPA_CHECK_NOT_INVOKED(f04, (0, 0));
    CPPA_CHECK_NOT_INVOKED(f04, (0, 42));
    CPPA_CHECK_NOT_INVOKED(f04, (42, 0));
    CPPA_CHECK_INVOKED(f04, (42, 42));

    auto f05 = _on(str2int).when(_x1 % 2 == 0) >> [&]() { invoked = "f05"; };
    CPPA_CHECK_NOT_INVOKED(f05, ("1"));
    CPPA_CHECK_INVOKED(f05, ("2"));

    auto f06 = _on(42, str2int).when(_x2 % 2 == 0) >> [&]() { invoked = "f06"; };
    CPPA_CHECK_NOT_INVOKED(f06, (0, "0"));
    CPPA_CHECK_NOT_INVOKED(f06, (42, "1"));
    CPPA_CHECK_INVOKED(f06, (42, "2"));

    int f07_val = 1;
    auto f07 = _on<int>().when(_x1 == gref(f07_val)) >> [&]() { invoked = "f07"; };
    CPPA_CHECK_NOT_INVOKED(f07, (0));
    CPPA_CHECK_INVOKED(f07, (1));
    CPPA_CHECK_NOT_INVOKED(f07, (2));
    ++f07_val;
    CPPA_CHECK_NOT_INVOKED(f07, (0));
    CPPA_CHECK_NOT_INVOKED(f07, (1));
    CPPA_CHECK_INVOKED(f07, (2));
    CPPA_CHECK(f07.invoke(make_any_tuple(2)));

    int f08_val = 666;
    auto f08 = _on<int>() >> [&](int& mref) { mref = 8; invoked = "f08"; };
    CPPA_CHECK_INVOKED(f08, (f08_val));
    CPPA_CHECK_EQUAL(8, f08_val);
    any_tuple f08_any_val = make_any_tuple(666);
    CPPA_CHECK(f08.invoke(f08_any_val));
    CPPA_CHECK_EQUAL(8, f08_any_val.get_as<int>(0));

    int f09_val = 666;
    auto f09 = _on(str2int, val<int>) >> [&](int& mref) { mref = 9; invoked = "f09"; };
    CPPA_CHECK_NOT_INVOKED(f09, ("hello lambda", f09_val));
    CPPA_CHECK_INVOKED(f09, ("0", f09_val));
    CPPA_CHECK_EQUAL(9, f09_val);
    any_tuple f09_any_val = make_any_tuple("0", 666);
    CPPA_CHECK(f09.invoke(f09_any_val));
    CPPA_CHECK_EQUAL(9, f09_any_val.get_as<int>(1));
    f09_any_val.get_as_mutable<int>(1) = 666;
    any_tuple f09_any_val_copy{f09_any_val};
    CPPA_CHECK_EQUAL(f09_any_val.at(0), f09_any_val_copy.at(0));
    // detaches f09_any_val from f09_any_val_copy
    CPPA_CHECK(f09.invoke(f09_any_val));
    CPPA_CHECK_EQUAL(9, f09_any_val.get_as<int>(1));
    CPPA_CHECK_EQUAL(666, f09_any_val_copy.get_as<int>(1));
    // no longer the same data
    CPPA_CHECK_NOT_EQUAL(f09_any_val.at(0), f09_any_val_copy.at(0));

    auto f10 =
    (
        _on<int>().when(_x1 < 10)    >> [&]() { invoked = "f10.0"; },
        _on<int>()                   >> [&]() { invoked = "f10.1"; },
        _on<std::string, anything>() >> [&](std::string&) { invoked = "f10.2"; }
    );

    CPPA_CHECK(f10(9));
    CPPA_CHECK_EQUAL("f10.0", invoked);
    CPPA_CHECK(f10(10));
    CPPA_CHECK_EQUAL("f10.1", invoked);
    CPPA_CHECK(f10("42"));
    CPPA_CHECK_EQUAL("f10.2", invoked);
    CPPA_CHECK(f10("42", 42));
    CPPA_CHECK(f10("a", "b", "c"));
    std::string foobar = "foobar";
    CPPA_CHECK(f10(foobar, "b", "c"));
    CPPA_CHECK(f10("a", static_cast<std::string const&>(foobar), "b", "c"));
    //CPPA_CHECK(f10(static_cast<std::string const&>(foobar), "b", "c"));

    int f11_fun = 0;
    auto f11 = pj_concat
    (
        _on<int>().when(_x1 == 1) >> [&]() { f11_fun =  1; },
        _on<int>().when(_x1 == 2) >> [&]() { f11_fun =  2; },
        _on<int>().when(_x1 == 3) >> [&]() { f11_fun =  3; },
        _on<int>().when(_x1 == 4) >> [&]() { f11_fun =  4; },
        _on<int>().when(_x1 == 5) >> [&]() { f11_fun =  5; },
        _on<int>().when(_x1 == 6) >> [&]() { f11_fun =  6; },
        _on<int>().when(_x1 == 7) >> [&]() { f11_fun =  7; },
        _on<int>().when(_x1 == 8) >> [&]() { f11_fun =  8; },
        _on<int>().when(_x1 >= 9) >> [&]() { f11_fun =  9; },
        _on(str2int)              >> [&]() { f11_fun = 10; },
        _on<std::string>()        >> [&]() { f11_fun = 11; }
    );

    CPPA_CHECK(f11(1));
    CPPA_CHECK_EQUAL(1, f11_fun);
    CPPA_CHECK(f11(3));
    CPPA_CHECK_EQUAL(3, f11_fun);
    CPPA_CHECK(f11(8));
    CPPA_CHECK_EQUAL(8, f11_fun);
    CPPA_CHECK(f11(10));
    CPPA_CHECK_EQUAL(9, f11_fun);
    CPPA_CHECK(f11("hello lambda"));
    CPPA_CHECK_EQUAL(11, f11_fun);
    CPPA_CHECK(f11("10"));
    CPPA_CHECK_EQUAL(10, f11_fun);

    auto old_pf =
    (
        on(42) >> []() { },
        on("abc") >> []() { },
        on<int, int>() >> []() { },
        on<anything>() >> []() { }
    );

    auto new_pf =
    (
        _on(42) >> []() { },
        _on(std::string("abc")) >> []() { },
        _on<int, int>() >> []() { },
        _on<anything>() >> []() { }
    );

    any_tuple testee[] = {
        make_cow_tuple(42),
        make_cow_tuple("abc"),
        make_cow_tuple("42"),
        make_cow_tuple(1, 2),
        make_cow_tuple(1, 2, 3)
    };

    cout << "old partial function implementation for 10,000,000 matches" << endl;
    {
        boost::progress_timer t0;
        for (size_t i = 0; i < (10000000 / sizeof(testee)); ++i)
        {
            for (auto& x : testee) { old_pf(x); }
        }
    }

    cout << "new partial function implementation for 1,000,000 matches" << endl;
    {
        boost::progress_timer t0;
        for (size_t i = 0; i < (10000000 / sizeof(testee)); ++i)
        {
            for (auto& x : testee) { new_pf.invoke(x); }
        }
    }

    cout << "old partial function with on() inside loop" << endl;
    {
        boost::progress_timer t0;
        for (size_t i = 0; i < (10000000 / sizeof(testee)); ++i)
        {
            auto tmp =
            (
                on(42) >> []() { },
                on("abc") >> []() { },
                on<int, int>() >> []() { },
                on<anything>() >> []() { }
            );
            for (auto& x : testee) { tmp(x); }
        }
    }

    cout << "new partial function with on() inside loop" << endl;
    {
        boost::progress_timer t0;
        for (size_t i = 0; i < (10000000 / sizeof(testee)); ++i)
        {
            auto tmp =
            (
                _on(42) >> []() { },
                _on(std::string("abc")) >> []() { },
                _on<int, int>() >> []() { },
                _on<anything>() >> []() { }
            );
            for (auto& x : testee) { tmp(x); }
        }
    }

    //exit(0);

    /*
    VERBOSE(f00(42, 42));
    VERBOSE(f01(42, 42));
    VERBOSE(f02(42, 42));
    VERBOSE(f02(42, 21));
    VERBOSE(f03(42, 42));

    cout << detail::demangle(typeid(f04).name()) << endl;

    VERBOSE(f04(42, 42));
    VERBOSE(f04(42, std::string("42")));

    VERBOSE(f05(42, 42));
    VERBOSE(f05(42, std::string("41")));
    VERBOSE(f05(42, std::string("42")));
    VERBOSE(f05(42, std::string("hello world!")));

    auto f06 = f04.or_else(_on<int, int>().when(_x2 > _x1) >> []() { });
    VERBOSE(f06(42, 42));
    VERBOSE(f06(1, 2));
    */

    /*
    auto f06 = _on<anything, int>() >> []() { };

    VERBOSE(f06(1));
    VERBOSE(f06(1.f, 2));
*/

    /*

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

    */

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
