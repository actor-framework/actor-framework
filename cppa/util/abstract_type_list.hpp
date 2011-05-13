#ifndef ABSTRACT_TYPE_LIST_HPP
#define ABSTRACT_TYPE_LIST_HPP

#include <iterator>

// forward declaration
namespace cppa { class uniform_type_info; }

namespace cppa { namespace util {

struct abstract_type_list
{

    class const_iterator : public std::iterator<std::bidirectional_iterator_tag,
                                                const uniform_type_info*>
    {

        const uniform_type_info* const* p;

     public:

        inline const_iterator(const uniform_type_info* const* x = 0) : p(x) { }

        const_iterator(const const_iterator&) = default;

        const_iterator& operator=(const const_iterator&) = default;

        inline bool operator==(const const_iterator& other) const
        {
            return p == other.p;
        }

        inline bool operator!=(const const_iterator& other) const
        {
            return p != other.p;
        }

        inline const uniform_type_info& operator*() const
        {
            return *(*p);
        }

        inline const uniform_type_info* operator->() const
        {
            return *p;
        }

        inline const_iterator& operator++()
        {
            ++p;
            return *this;
        }

        const_iterator operator++(int);

        inline const_iterator& operator--()
        {
            --p;
            return *this;
        }

        const_iterator operator--(int);

    };

    virtual ~abstract_type_list();

    virtual abstract_type_list* copy() const = 0;

    virtual const_iterator begin() const = 0;

    virtual const_iterator end() const = 0;

    virtual const uniform_type_info* at(std::size_t pos) const = 0;

};

} } // namespace cppa::util

#endif // ABSTRACT_TYPE_LIST_HPP
