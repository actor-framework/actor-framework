#ifndef ANY_TUPLE_ITERATOR_HPP
#define ANY_TUPLE_ITERATOR_HPP

#include "cppa/any_tuple.hpp"

namespace cppa { namespace util {

class any_tuple_iterator
{

    const any_tuple& m_data;
    size_t m_pos;

 public:

    any_tuple_iterator(const any_tuple& data, size_t pos = 0);

    inline bool at_end() const;

    template<typename T>
    inline const T& value() const;

    inline const cppa::uniform_type_info& type() const;

    inline size_t position() const;

    inline void next();

};

inline bool any_tuple_iterator::at_end() const
{
    return m_pos >= m_data.size();
}

template<typename T>
inline const T& any_tuple_iterator::value() const
{
    return *reinterpret_cast<const T*>(m_data.at(m_pos));
}

inline const uniform_type_info& any_tuple_iterator::type() const
{
    return m_data.utype_info_at(m_pos);
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
