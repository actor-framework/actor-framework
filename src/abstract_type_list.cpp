#include "cppa/util/abstract_type_list.hpp"

namespace cppa { namespace util {

abstract_type_list::~abstract_type_list()
{
}

abstract_type_list::const_iterator
abstract_type_list::const_iterator::operator++(int)
{
    const_iterator tmp(*this);
    operator++();
    return tmp;
}

abstract_type_list::const_iterator
abstract_type_list::const_iterator::operator--(int)
{
    const_iterator tmp(*this);
    operator--();
    return tmp;
}

} } // namespace cppa::util
