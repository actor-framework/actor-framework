#ifndef FIXED_VECTOR_HPP
#define FIXED_VECTOR_HPP

#include <algorithm>
#include <stdexcept>

namespace cppa { namespace util {

// @warning does not perform any bound checks
template<typename T, size_t MaxSize>
class fixed_vector
{

    size_t m_size;

    // support for MaxSize == 0
    T m_data[(MaxSize > 0) ? MaxSize : 1];

 public:

    typedef size_t size_type;

    typedef T& reference;
    typedef const T& const_reference;

    typedef T* iterator;
    typedef const T* const_iterator;

    constexpr fixed_vector() : m_size(0)
    {
    }

    fixed_vector(const fixed_vector& other) : m_size(other.m_size)
    {
        std::copy(other.m_data, other.m_data + other.m_size, m_data);
    }

    inline size_type size() const
    {
        return m_size;
    }

    inline void clear()
    {
        m_size = 0;
    }

    inline bool full() const
    {
        return m_size == MaxSize;
    }

    inline void push_back(const T& what)
    {
        m_data[m_size++] = what;
    }

    inline void push_back(T&& what)
    {
        m_data[m_size++] = std::move(what);
    }

    inline reference operator[](size_t pos)
    {
        return m_data[pos];
    }

    inline const_reference operator[](size_t pos) const
    {
        return m_data[pos];
    }

    inline iterator begin()
    {
        return m_data;
    }

    inline const_iterator begin() const
    {
        return m_data;
    }

    inline iterator end()
    {
        return (static_cast<T*>(m_data) + m_size);
    }

    inline const_iterator end() const
    {
        return (static_cast<T*>(m_data) + m_size);
    }

    template<class InputIterator>
    inline void insert(iterator pos,
                       InputIterator first,
                       InputIterator last)
    {
        if (pos == end())
        {
            for (InputIterator i = first; i != last; ++i)
            {
                push_back(*i);
            }
        }
        else
        {
            throw std::runtime_error("not implemented fixed_vector::insert");
        }
    }

};

} } // namespace cppa::util

#endif // FIXED_VECTOR_HPP
