#include "cppa/detail/abstract_tuple.hpp"

namespace cppa { namespace detail {

bool abstract_tuple::equal_to(const abstract_tuple &other) const
{
    if (this == &other) return true;
    if (size() != other.size()) return false;
    for (size_t i = 0; i < size(); ++i)
    {
        const cppa::uniform_type_info& uti = utype_info_at(i);
        if (uti != other.utype_info_at(i)) return false;
        auto lhs = at(i);
        auto rhs = other.at(i);
        // compare first addresses, then values
        if (lhs != rhs && !(uti.equal(lhs, rhs))) return false;
    }
    return true;
}

} } // namespace cppa::detail
