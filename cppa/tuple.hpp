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
#include "cppa/detail/implicit_conversions.hpp"

namespace cppa {

// forward declaration
class any_tuple;
class local_actor;

/**
 * @ingroup CopyOnWrite
 * @brief A fixed-length copy-on-write tuple.
 */
template<typename... ElementTypes>
class tuple
{

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

 public:

    /**
     * @brief Initializes each element with its default constructor.
     */
    tuple() : m_vals(new vals_t)
    {
    }

    /**
     * @brief Initializes the tuple with @p args.
     * @param args Initialization values.
     */
    tuple(ElementTypes const&... args) : m_vals(new vals_t(args...))
    {
    }

    /**
     * @brief Gets the size of this tuple.
     * @returns <tt>sizeof...(ElementTypes)</tt>.
     */
    inline size_t size() const
    {
        return sizeof...(ElementTypes);
        //return m_vals->size();
    }

    /**
     * @brief Gets a pointer to the internal data.
     * @returns A const void pointer to the <tt>N</tt>th element.
     */
    inline void const* at(size_t p) const
    {
        return m_vals->at(p);
    }

    /**
     * @brief Gets {@link uniform_type_info uniform type information}
     *        of an element.
     * @returns The uniform type of the <tt>N</tt>th element.
     */
    inline uniform_type_info const* utype_at(size_t p) const
    {
        return m_vals->utype_info_at(p);
    }

#   ifdef CPPA_DOCUMENTATION
    /**
     * @brief Gets the internal data.
     * @returns A pointer to the internal data representation.
     */
    cow_ptr<InternalData> vals() const;
#   else
    inline cow_ptr<vals_t> const& vals() const
    {
        return m_vals;
    }
#   endif

};

template<typename TypeList>
struct tuple_type_from_type_list;

template<typename... Types>
struct tuple_type_from_type_list<util::type_list<Types...>>
{
    typedef tuple<Types...> type;
};

#ifdef CPPA_DOCUMENTATION

/**
 * @ingroup CopyOnWrite
 * @brief Gets a const-reference to the <tt>N</tt>th element of @p tup.
 * @param tup The tuple object.
 * @returns A const-reference of type T, whereas T is the type of the
 *          <tt>N</tt>th element of @p tup.
 * @relates tuple
 */
template<size_t N, typename T>
T const& get(tuple<...> const& tup);

/**
 * @ingroup CopyOnWrite
 * @brief Gets a reference to the <tt>N</tt>th element of @p tup.
 * @param tup The tuple object.
 * @returns A reference of type T, whereas T is the type of the
 *          <tt>N</tt>th element of @p tup.
 * @note Detaches @p tup if there are two or more references to the tuple data.
 * @relates tuple
 */
template<size_t N, typename T>
T& get_ref(tuple<...>& tup);

/**
 * @ingroup ImplicitConversion
 * @brief Creates a new tuple from @p args.
 * @param args Values for the tuple elements.
 * @returns A tuple object containing the values @p args.
 * @relates tuple
 */
template<typename... Types>
tuple<Types...> make_tuple(Types const&... args);

#else

template<size_t N, typename... Types>
const typename util::at<N, Types...>::type& get(tuple<Types...> const& tup)
{
    return get<N>(tup.vals()->data());
}

template<size_t N, typename... Types>
typename util::at<N, Types...>::type& get_ref(tuple<Types...>& tup)
{
    return get_ref<N>(tup.m_vals->data_ref());
}

template<typename... Types>
typename tuple_type_from_type_list<
    typename util::type_list_apply<util::type_list<Types...>,
                                   detail::implicit_conversions>::type>::type
make_tuple(Types const&... args)
{
    return { args... };
}

#endif

/**
 * @brief Compares two tuples.
 * @param lhs First tuple object.
 * @param rhs Second tuple object.
 * @returns @p true if @p lhs and @p rhs are equal; otherwise @p false.
 * @relates tuple
 */
template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(tuple<LhsTypes...> const& lhs,
                       tuple<RhsTypes...> const& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

/**
 * @brief Compares two tuples.
 * @param lhs First tuple object.
 * @param rhs Second tuple object.
 * @returns @p true if @p lhs and @p rhs are not equal; otherwise @p false.
 * @relates tuple
 */
template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(tuple<LhsTypes...> const& lhs,
                       tuple<RhsTypes...> const& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif
