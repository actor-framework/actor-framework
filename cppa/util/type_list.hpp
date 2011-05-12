#ifndef LIBCPPA_UTIL_TYPE_LIST_HPP
#define LIBCPPA_UTIL_TYPE_LIST_HPP

#include <typeinfo>
#include "cppa/any_type.hpp"
//#include "cppa/uniform_type_info.hpp"

#include "cppa/util/void_type.hpp"
#include "cppa/util/abstract_type_list.hpp"

namespace cppa {

// forward declarations
class uniform_type_info;
const uniform_type_info* uniform_typeid(const std::type_info&);

} // namespace cppa

namespace cppa { namespace util {

template<typename... Types> struct type_list;

template<>
struct type_list<>
{
    typedef void_type head_type;
    typedef type_list<> tail_type;
    static const std::size_t type_list_size = 0;
};

template<typename Head, typename... Tail>
struct type_list<Head, Tail...> : abstract_type_list
{

    typedef Head head_type;

    typedef type_list<Tail...> tail_type;

    static const std::size_t type_list_size = tail_type::type_list_size + 1;

    type_list() { init<type_list>(m_arr); }

    virtual const_iterator begin() const
    {
        return m_arr;
    }

    virtual const_iterator end() const
    {
        return m_arr + type_list_size;
    }

    virtual const uniform_type_info* at(std::size_t pos) const
    {
        return m_arr[pos];
    }

    virtual type_list* copy() const
    {
        return new type_list;
    }

    template<typename TypeList>
    static void init(const uniform_type_info** what)
    {
        what[0] = uniform_typeid(typeid(typename TypeList::head_type));
        if (TypeList::type_list_size > 1)
        {
            ++what;
            init<typename TypeList::tail_type>(what);
        }
        /*
        what[0] = &uniform_type_info<typename TypeList::head_type>();
        if (TypeList::type_list_size > 1)
        {
            ++what;
            init<typename TypeList::tail_type>(what);
        }
    */
    }

 private:

    const uniform_type_info* m_arr[type_list_size];

};

} } // namespace cppa::util

#endif // LIBCPPA_UTIL_TYPE_LIST_HPP
