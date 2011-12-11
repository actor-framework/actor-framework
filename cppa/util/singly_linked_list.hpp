#ifndef SINGLY_LINKED_LIST_HPP
#define SINGLY_LINKED_LIST_HPP

#include <utility>

namespace cppa { namespace util {

template<typename T>
class singly_linked_list
{

    T* m_head;
    T* m_tail;

 public:

    typedef T element_type;

    singly_linked_list() : m_head(nullptr), m_tail(nullptr) { }

    ~singly_linked_list()
    {
        clear();
    }

    inline bool empty() const { return m_head == nullptr; }

    void push_back(element_type* what)
    {
        what->next = nullptr;
        if (m_tail)
        {
            m_tail->next = what;
            m_tail = what;
        }
        else
        {
            m_head = m_tail = what;
        }
    }

    std::pair<element_type*, element_type*> take()
    {
        element_type* first = m_head;
        element_type* last = m_tail;
        m_head = m_tail = nullptr;
        return { first, last };
    }

    void clear()
    {
        while (m_head)
        {
            T* next = m_head->next;
            delete m_head;
            m_head = next;
        }
        m_head = m_tail = nullptr;
    }

};

} } // namespace cppa::util

#endif // SINGLY_LINKED_LIST_HPP
