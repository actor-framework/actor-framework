#ifndef LIBCPPA_UTIL_TYPE_LIST_HPP
#define LIBCPPA_UTIL_TYPE_LIST_HPP

#include <typeinfo>
#include "cppa/any_type.hpp"

#include "cppa/util/void_type.hpp"
#include "cppa/util/abstract_type_list.hpp"

namespace cppa {

// forward declarations
class uniform_type_info;
const uniform_type_info* uniform_typeid(const std::type_info&);

} // namespace cppa

namespace cppa { namespace util {

class type_list_iterator : public abstract_type_list::abstract_iterator
{

 public:

    typedef const uniform_type_info* const* ptr_type;

    type_list_iterator(ptr_type begin, ptr_type end) : m_pos(begin), m_end(end)
    {
    }

    type_list_iterator(const type_list_iterator&) = default;

    bool next()
    {
        return ++m_pos != m_end;
    }

    const uniform_type_info& get() const
    {
        return *(*m_pos);
    }

    abstract_type_list::abstract_iterator* copy() const
    {
        return new type_list_iterator(*this);
    }

 private:

    const uniform_type_info* const* m_pos;
    const uniform_type_info* const* m_end;

};

template<typename... Types> struct type_list;

template<>
struct type_list<>
{
    typedef void_type head_type;
    typedef type_list<> tail_type;
    static const size_t size = 0;
};

template<typename Head, typename... Tail>
struct type_list<Head, Tail...> : abstract_type_list
{

    typedef Head head_type;

    typedef type_list<Tail...> tail_type;

    static const size_t size =  sizeof...(Tail) + 1;

    type_list()
    {
        init<type_list>(m_arr);
    }

    virtual const_iterator begin() const
    {
        return new type_list_iterator(m_arr, m_arr + size);
    }

    virtual const uniform_type_info& at(size_t pos) const
    {
        return *m_arr[pos];
    }

    virtual type_list* copy() const
    {
        return new type_list;
    }

    template<typename TypeList>
    static void init(const uniform_type_info** what)
    {
        what[0] = uniform_typeid(typeid(typename TypeList::head_type));
        if (TypeList::size > 1)
        {
            ++what;
            init<typename TypeList::tail_type>(what);
        }
    }

 private:

    const uniform_type_info* m_arr[size];

};

} } // namespace cppa::util

#endif // LIBCPPA_UTIL_TYPE_LIST_HPP
