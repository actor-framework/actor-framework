#ifndef GET_BEHAVIOR_HPP
#define GET_BEHAVIOR_HPP

#include <type_traits>

#include "cppa/invoke.hpp"
#include "cppa/util/rm_ref.hpp"
#include "cppa/detail/tdata.hpp"
#include "cppa/actor_behavior.hpp"

namespace cppa { namespace detail {

// default: <true, false, F>
template<bool IsFunctionPtr, bool HasArguments, typename F, typename... Args>
class ftor_behavior : public actor_behavior
{

    F m_fun;

 public:

    ftor_behavior(F ptr) : m_fun(ptr) { }

    virtual void act() { m_fun(); }

};

template<typename F, typename... Args>
class ftor_behavior<true, true, F, Args...>  : public actor_behavior
{

    static_assert(sizeof...(Args) > 0, "sizeof...(Args) == 0");

    F m_fun;
    tdata<Args...> m_args;

 public:

    ftor_behavior(F ptr, const Args&... args) : m_fun(ptr), m_args(args...) { }

    virtual void act() { invoke(m_fun, m_args); }

};

template<typename F>
class ftor_behavior<false, false, F> : public actor_behavior
{

    F m_fun;

 public:

    ftor_behavior(const F& arg) : m_fun(arg) { }

    ftor_behavior(F&& arg) : m_fun(std::move(arg)) { }

    virtual void act() { m_fun(); }

};

template<typename F, typename... Args>
class ftor_behavior<false, true, F, Args...>  : public actor_behavior
{

    static_assert(sizeof...(Args) > 0, "sizeof...(Args) == 0");

    F m_fun;
    tdata<Args...> m_args;

 public:

    ftor_behavior(const F& f, const Args&... args) : m_fun(f), m_args(args...)
    {
    }

    ftor_behavior(F&& f,const Args&... args) : m_fun(std::move(f))
                                             , m_args(args...)
    {
    }

    virtual void act() { invoke(m_fun, m_args); }

};

template<typename R>
actor_behavior* get_behavior(std::integral_constant<bool,true>, R (*fptr)())
{
    static_assert(std::is_convertible<R, actor_behavior*>::value == false,
                  "Spawning a function returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that function?");
    return new ftor_behavior<true, false, R (*)()>(fptr);
}

template<typename F>
actor_behavior* get_behavior(std::integral_constant<bool,false>, F&& ftor)
{
    static_assert(std::is_convertible<decltype(ftor()), actor_behavior*>::value == false,
                  "Spawning a functor returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that functor?");
    typedef typename util::rm_ref<F>::type ftype;
    return new ftor_behavior<false, false, ftype>(std::forward<F>(ftor));
}

template<typename F, typename Arg0, typename... Args>
actor_behavior* get_behavior(std::integral_constant<bool,true>,
                             F fptr,
                             const Arg0& arg0,
                             const Args&... args)
{
    static_assert(std::is_convertible<decltype(fptr(arg0, args...)), actor_behavior*>::value == false,
                  "Spawning a function returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that function?");
    typedef ftor_behavior<true, true, F, Arg0, Args...> impl;
    return new impl(fptr, arg0, args...);
}

template<typename F, typename Arg0, typename... Args>
actor_behavior* get_behavior(std::integral_constant<bool,false>,
                             F ftor,
                             const Arg0& arg0,
                             const Args&... args)
{
    static_assert(std::is_convertible<decltype(ftor(arg0, args...)), actor_behavior*>::value == false,
                  "Spawning a functor returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that functor?");
    typedef typename util::rm_ref<F>::type ftype;
    typedef ftor_behavior<false, true, ftype, Arg0, Args...> impl;
    return new impl(std::forward<F>(ftor), arg0, args...);
}

} } // namespace cppa::detail

#endif // GET_BEHAVIOR_HPP
