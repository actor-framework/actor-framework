#ifndef CPPA_TUPLE_HPP
#define CPPA_TUPLE_HPP

#include <cstddef>
#include <string>
#include <typeinfo>
#include <type_traits>

#include "cppa/get.hpp"
#include "cppa/actor.hpp"
#include "cppa/cow_ptr.hpp"
#include "cppa/ref_counted.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/replace_type.hpp"
#include "cppa/util/is_comparable.hpp"
#include "cppa/util/compare_tuples.hpp"
#include "cppa/util/eval_type_list.hpp"
#include "cppa/util/type_list_apply.hpp"
#include "cppa/util/is_legal_tuple_type.hpp"

#include "cppa/detail/tuple_vals.hpp"

namespace cppa { class local_actor; }

namespace cppa { namespace detail {

/**
 * @brief <tt>is_array_of<T,U>::value == true</tt> if and only
 *        if T is an array of U.
 */
template<typename T, typename U>
struct is_array_of
{
    typedef typename std::remove_all_extents<T>::type step1_type;
    typedef typename std::remove_cv<step1_type>::type step2_type;
    static const bool value =    std::is_array<T>::value
                              && std::is_same<step2_type, U>::value;
};


template<typename T>
struct chars_to_string
{
    typedef typename util::replace_type<T, std::string,
                                        std::is_same<T, const char*>,
                                        std::is_same<T, char*>,
                                        is_array_of<T, char>,
                                        is_array_of<T, const char> >::type
            subtype1;

    typedef typename util::replace_type<subtype1, std::u16string,
                                        std::is_same<subtype1, const char16_t*>,
                                        std::is_same<subtype1, char16_t*>,
                                        is_array_of<subtype1, char16_t>>::type
            subtype2;

    typedef typename util::replace_type<subtype2, std::u32string,
                                        std::is_same<subtype2, const char32_t*>,
                                        std::is_same<subtype2, char32_t*>,
                                        is_array_of<subtype2, char32_t>>::type
            subtype3;

    typedef typename util::replace_type<subtype3, actor_ptr,
                                        std::is_same<actor*,T>,
                                        std::is_same<local_actor*,T>>::type
            type;
};

} } // namespace cppa::detail

namespace cppa {

// forward declaration
class any_tuple;

/**
 * @brief Describes a fixed-length tuple.
 */
template<typename... ElementTypes>
class tuple
{

    //friend class any_tuple;

    template<size_t N, typename... Types>
    friend typename util::at<N, Types...>::type& get_ref(tuple<Types...>&);


 public:

    typedef util::type_list<ElementTypes...> element_types;

 private:

    static_assert(sizeof...(ElementTypes) > 0, "tuple is empty");

    static_assert(util::eval_type_list<element_types,
                                       util::is_legal_tuple_type>::value,
                  "illegal types in tuple definition: "
                  "pointers and references are prohibited");

    typedef detail::tuple_vals<ElementTypes...> vals_t;

    cow_ptr<vals_t> m_vals;

    static bool compare_vals(const detail::tdata<>& v0,
                             const detail::tdata<>& v1)
    {
        return true;
    }

    template<typename Vals0, typename Vals1>
    static bool compare_vals(const Vals0& v0, const Vals1& v1)
    {
        typedef typename Vals0::head_type lhs_type;
        typedef typename Vals1::head_type rhs_type;
        static_assert(util::is_comparable<lhs_type, rhs_type>::value,
                      "Types are not comparable");
        return v0.head == v1.head && compare_vals(v0.tail(), v1.tail());
    }

 public:

    // enable use of tuple as type_list
    typedef typename element_types::head_type head_type;
    typedef typename element_types::tail_type tail_type;

    tuple() : m_vals(new vals_t)
    {
    }

    tuple(const ElementTypes&... args) : m_vals(new vals_t(args...))
    {
    }

    size_t size() const
    {
        return m_vals->size();
    }

    const void* at(size_t p) const
    {
        return m_vals->at(p);
    }

    const uniform_type_info* utype_at(size_t p) const
    {
        return m_vals->utype_info_at(p);
    }

    const util::abstract_type_list& types() const
    {
        return m_vals->types();
    }

    const cow_ptr<vals_t>& vals() const
    {
        return m_vals;
    }

    template<typename... Args>
    bool equal_to(const tuple<Args...>& other) const
    {
        static_assert(sizeof...(ElementTypes) == sizeof...(Args),
                      "Can't compare tuples of different size");
        return compare_vals(vals()->data(), other.vals()->data());
    }

};

template<size_t N, typename... Types>
const typename util::at<N, Types...>::type&
get(const tuple<Types...>& t)
{
    return get<N>(t.vals()->data());
}

template<size_t N, typename... Types>
typename util::at<N, Types...>::type&
get_ref(tuple<Types...>& t)
{
    return get_ref<N>(t.m_vals->data_ref());
}

template<typename TypeList>
struct tuple_type_from_type_list;

template<typename... Types>
struct tuple_type_from_type_list<util::type_list<Types...>>
{
    typedef tuple<Types...> type;
};

template<typename... Types>
typename tuple_type_from_type_list<
        typename util::type_list_apply<util::type_list<Types...>,
                                       detail::chars_to_string>::type>::type
make_tuple(const Types&... args)
{
    return { args... };
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const tuple<LhsTypes...>& lhs,
                       const tuple<RhsTypes...>& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const tuple<LhsTypes...>& lhs,
                       const tuple<RhsTypes...>& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif
