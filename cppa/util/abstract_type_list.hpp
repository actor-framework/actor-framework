#ifndef ABSTRACT_TYPE_LIST_HPP
#define ABSTRACT_TYPE_LIST_HPP

#include <memory>

// forward declaration
namespace cppa { class uniform_type_info; }

namespace cppa { namespace util {

struct abstract_type_list
{

    class abstract_iterator
    {

     public:

        virtual ~abstract_iterator();

        /**
         * @brief Increases the iterator position.
         * @returns @c false if the iterator is at the end; otherwise @c true.
         */
        virtual bool next() = 0;

        virtual const uniform_type_info& get() const = 0;

        virtual abstract_iterator* copy() const = 0;

    };

    class const_iterator
    {

        abstract_iterator* m_iter;

     public:

        const_iterator(abstract_iterator* x = nullptr) : m_iter(x) { }

        const_iterator(const const_iterator& other)
            : m_iter((other.m_iter) ? other.m_iter->copy() : nullptr)
        {
        }

        const_iterator(const_iterator&& other) : m_iter(other.m_iter)
        {
            other.m_iter = nullptr;
        }

        const_iterator& operator=(const const_iterator& other)
        {
            delete m_iter;
            m_iter = (other.m_iter) ? other.m_iter->copy() : nullptr;
            return *this;
        }

        const_iterator& operator=(const_iterator&& other)
        {
            delete m_iter;
            m_iter = other.m_iter;
            other.m_iter = nullptr;
            return *this;
        }

        ~const_iterator()
        {
            delete m_iter;
        }

        bool operator==(const const_iterator& other) const
        {
            return m_iter == other.m_iter;
        }

        inline bool operator!=(const const_iterator& other) const
        {
            return !(*this == other);
        }

        const uniform_type_info& operator*() const
        {
            return m_iter->get();
        }

        const uniform_type_info* operator->() const
        {
            return &(m_iter->get());
        }

        const_iterator& operator++()
        {
            if (!m_iter->next())
            {
                delete m_iter;
                m_iter = nullptr;
            }
            return *this;
        }

    };

    virtual ~abstract_type_list();

    virtual const_iterator begin() const = 0;

    virtual const_iterator end() const;

};

} } // namespace cppa::util

#endif // ABSTRACT_TYPE_LIST_HPP
