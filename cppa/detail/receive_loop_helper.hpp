#ifndef RECEIVE_LOOP_HELPER_HPP
#define RECEIVE_LOOP_HELPER_HPP

#include <new>

#include "cppa/receive.hpp"
#include "cppa/invoke_rules.hpp"


namespace cppa { namespace detail {

template<typename Statement>
struct receive_while_helper
{
    Statement m_stmt;
    receive_while_helper(Statement&& stmt) : m_stmt(std::move(stmt))
    {
    }

    void operator()(invoke_rules& rules)
    {
        while (m_stmt())
        {
            receive(rules);
        }
    }

    void operator()(invoke_rules&& rules)
    {
        invoke_rules tmp(std::move(rules));
        (*this)(tmp);
    }

    void operator()(timed_invoke_rules& rules)
    {
        while (m_stmt())
        {
            receive(rules);
        }
    }

    void operator()(timed_invoke_rules&& rules)
    {
        timed_invoke_rules tmp(std::move(rules));
        (*this)(tmp);
    }

    template<typename Arg0, typename... Args>
    void operator()(invoke_rules& rules, Arg0&& arg0, Args&&... args)
    {
        (*this)(rules.splice(std::forward<Arg0>(arg0)),
                std::forward<Args>(args)...);
    }

    template<typename Arg0, typename... Args>
    void operator()(invoke_rules&& rules, Arg0&& arg0, Args&&... args)
    {
        invoke_rules tmp(std::move(rules));
        (*this)(tmp.splice(std::forward<Arg0>(arg0)),
                std::forward<Args>(args)...);
    }
};

class do_receive_helper
{

    bool m_timed;

    union
    {
        invoke_rules m_rules;
        timed_invoke_rules m_trules;
    };

    inline void init(timed_invoke_rules&& ti_rules)
    {
        m_timed = true;
        m_rules.~invoke_rules();
        new (&m_trules) timed_invoke_rules(std::move(ti_rules));
    }

    inline void init(invoke_rules&) { }

    template<typename Arg0, typename... Args>
    inline void init(invoke_rules& rules, Arg0&& arg0, Args&&... args)
    {
        init(rules.splice(std::forward<Arg0>(arg0)),
             std::forward<Args>(args)...);
    }

 public:

    template<typename... Args>
    do_receive_helper(invoke_rules&& rules, Args&&... args) : m_timed(false)
    {
        new (&m_rules) invoke_rules(std::move(rules));
        init(m_rules, std::forward<Args>(args)...);
    }

    do_receive_helper(timed_invoke_rules&& rules) : m_timed(true)
    {
        new (&m_trules) timed_invoke_rules(std::move(rules));
    }

    do_receive_helper(do_receive_helper&& other) : m_timed(other.m_timed)
    {
        if (other.m_timed)
        {
            new (&m_trules) timed_invoke_rules(std::move(other.m_trules));
        }
        else
        {
            new (&m_rules) invoke_rules(std::move(other.m_rules));
        }
    }

    ~do_receive_helper()
    {
        if (m_timed) m_trules.~timed_invoke_rules();
        else m_rules.~invoke_rules();
    }

    template<typename Statement>
    void until(Statement&& stmt)
    {
        static_assert(std::is_same<bool, decltype(stmt())>::value,
                      "functor or function does not return a boolean");
        if (m_timed)
        {
            do
            {
                receive(m_trules);
            }
            while (stmt() == false);
        }
        else
        {
            do
            {
                receive(m_rules);
            }
            while (stmt() == false);
        }
    }

};

} } // namespace cppa::detail

#endif // RECEIVE_LOOP_HELPER_HPP
