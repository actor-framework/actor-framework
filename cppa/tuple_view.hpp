#ifndef TUPLE_VIEW_HPP
#define TUPLE_VIEW_HPP

#include <vector>

#include "cppa/tuple.hpp"

#include "cppa/util/type_list.hpp"
#include "cppa/util/a_matches_b.hpp"
#include "cppa/util/compare_tuples.hpp"

#include "cppa/detail/decorated_tuple.hpp"

namespace cppa {

// forward declaration
class any_tuple;
// needed to implement constructor
const cow_ptr<detail::abstract_tuple>& vals_of(const any_tuple&);

/**
 * @brief Describes a view of an fixed-length tuple.
 */
template<typename... ElementTypes>
class tuple_view
{

 public:

    typedef util::type_list<ElementTypes...> element_types;

    static_assert(sizeof...(ElementTypes) > 0,
                  "could not declare an empty tuple_view");

    typedef cow_ptr<detail::abstract_tuple> vals_t;

    tuple_view(const vals_t& vals, std::vector<size_t>&& mappings)
        : m_vals(new detail::decorated_tuple<ElementTypes...>(vals, mappings))
    {
    }

    tuple_view(const tuple_view&) = default;

    tuple_view(tuple_view&& other) : m_vals(std::move(other.m_vals))
    {
    }

    const vals_t& vals() const { return m_vals; }

    typedef typename element_types::head_type head_type;

    typedef typename element_types::tail_type tail_type;

    const element_types& types() const { return m_types; }

    template<size_t N>
    const typename util::type_at<N, element_types>::type& get() const
    {
        static_assert(N < sizeof...(ElementTypes), "N >= size()");
        return *reinterpret_cast<const typename util::type_at<N, element_types>::type*>(m_vals->at(N));
    }

    template<size_t N>
    typename util::type_at<N, element_types>::type& get_ref()
    {
        static_assert(N < sizeof...(ElementTypes), "N >= size()");
        return *reinterpret_cast<typename util::type_at<N, element_types>::type*>(m_vals->mutable_at(N));
    }

    size_t size() const { return m_vals->size(); }

 private:

    vals_t m_vals;
    element_types m_types;

};

template<size_t N, typename... Types>
const typename util::type_at<N, util::type_list<Types...>>::type&
get(const tuple_view<Types...>& t)
{
    static_assert(N < sizeof...(Types), "N >= t.size()");
    typedef typename util::type_at<N, util::type_list<Types...>>::type result_t;
    return *reinterpret_cast<const result_t*>(t.vals()->at(N));
    //return t.get<N>();
}

template<size_t N, typename... Types>
typename util::type_at<N, util::type_list<Types...>>::type&
get_ref(tuple_view<Types...>& t)
{
    static_assert(N < sizeof...(Types), "N >= t.size()");
    typedef typename util::type_at<N, util::type_list<Types...>>::type result_t;
    return *reinterpret_cast<result_t*>(t.vals()->mutable_at(N));
    //return t.get_ref<N>();
}

template<typename TypeList>
struct tuple_view_type_from_type_list;

template<typename... Types>
struct tuple_view_type_from_type_list<util::type_list<Types...>>
{
    typedef tuple_view<Types...> type;
};

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const tuple_view<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const tuple<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(const tuple_view<LhsTypes...>& lhs,
                       const tuple<RhsTypes...>& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const tuple_view<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const tuple<LhsTypes...>& lhs,
                       const tuple_view<RhsTypes...>& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(const tuple_view<LhsTypes...>& lhs,
                       const tuple<RhsTypes...>& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // TUPLE_VIEW_HPP
