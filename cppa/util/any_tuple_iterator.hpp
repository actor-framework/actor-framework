#ifndef ANY_TUPLE_ITERATOR_HPP
#define ANY_TUPLE_ITERATOR_HPP

#include "cppa/any_tuple.hpp"

namespace cppa { namespace util {

class any_tuple_iterator
{

    any_tuple const& m_data;
    size_t m_pos;

 public:

    any_tuple_iterator(any_tuple const& data, size_t pos = 0);

    inline bool at_end() const;

    template<typename T>
    inline T const& value() const;

    inline void const* value_ptr() const;

    inline cppa::uniform_type_info const& type() const;

    inline size_t position() const;

    inline void next();

};

inline bool any_tuple_iterator::at_end() const
{
    return m_pos >= m_data.size();
}

template<typename T>
inline T const& any_tuple_iterator::value() const
{
    return *reinterpret_cast<T const*>(m_data.at(m_pos));
}

inline uniform_type_info const& any_tuple_iterator::type() const
{
    return m_data.utype_info_at(m_pos);
}

inline void const* any_tuple_iterator::value_ptr() const
{
    return m_data.at(m_pos);
}

inline size_t any_tuple_iterator::position() const
{
    return m_pos;
}

inline void any_tuple_iterator::next()
{
    ++m_pos;
}


} } // namespace cppa::util

#endif // ANY_TUPLE_ITERATOR_HPP
