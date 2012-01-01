#ifndef SERIALIZE_TUPLE_HPP
#define SERIALIZE_TUPLE_HPP

#include <cstddef>

#include "cppa/uniform_type_info.hpp"
#include "cppa/util/type_list.hpp"

namespace cppa { class serializer; }

namespace cppa { namespace detail {

template<typename List, size_t Pos = 0>
struct serialize_tuple
{
    template<typename T>
    inline static void _(serializer& s, T const* tup)
    {
        s << uniform_typeid<typename List::head_type>()->name()
          << *reinterpret_cast<const typename List::head_type*>(tup->at(Pos));
        serialize_tuple<typename List::tail_type, Pos + 1>::_(s, tup);
    }
};

template<size_t Pos>
struct serialize_tuple<util::type_list<>, Pos>
{
    template<typename T>
    inline static void _(serializer&, T const*) { }
};

} } // namespace cppa::detail

#endif // SERIALIZE_TUPLE_HPP
