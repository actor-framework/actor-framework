#ifndef DELEGATE_HPP
#define DELEGATE_HPP

namespace cppa { namespace detail {

class delegate
{

    typedef void (*void_fun)(void*, void*);

    void_fun m_fun;
    void* m_arg1;
    void* m_arg2;

 public:

    template<typename Arg1, typename Arg2, typename Function>
    delegate(Function* fun, Arg1* a1, Arg2* a2)
        : m_fun(reinterpret_cast<void_fun>(fun))
        , m_arg1(reinterpret_cast<void*>(a1))
        , m_arg2(reinterpret_cast<void*>(a2))
    {
    }

    template<typename Arg1, typename Arg2, typename Function>
    void reset(Function* fun, Arg1* a1, Arg2* a2)
    {
        m_fun = reinterpret_cast<void_fun>(fun);
        m_arg1 = reinterpret_cast<void*>(a1);
        m_arg2 = reinterpret_cast<void*>(a2);
    }

    void operator()();

};

} } // namespace cppa::detail

#endif // DELEGATE_HPP
