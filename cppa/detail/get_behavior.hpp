#ifndef GET_BEHAVIOR_HPP
#define GET_BEHAVIOR_HPP

#include <type_traits>

#include "cppa/invoke.hpp"
#include "cppa/util/rm_ref.hpp"
#include "cppa/detail/tdata.hpp"
#include "cppa/scheduled_actor.hpp"

namespace cppa { namespace detail {

// default: <true, false, F>
template<bool IsFunctionPtr, bool HasArguments, typename F, typename... Args>
class ftor_behavior : public scheduled_actor
{

    F m_fun;

 public:

    ftor_behavior(F ptr) : m_fun(ptr) { }

    virtual void act() { m_fun(); }

};

template<typename F, typename... Args>
class ftor_behavior<true, true, F, Args...>  : public scheduled_actor
{

    static_assert(sizeof...(Args) > 0, "sizeof...(Args) == 0");

    F m_fun;
    tdata<Args...> m_args;

 public:

    ftor_behavior(F ptr, Args const&... args) : m_fun(ptr), m_args(args...) { }

    virtual void act() { invoke(m_fun, m_args); }

};

template<typename F>
class ftor_behavior<false, false, F> : public scheduled_actor
{

    F m_fun;

 public:

    ftor_behavior(F const& arg) : m_fun(arg) { }

    ftor_behavior(F&& arg) : m_fun(std::move(arg)) { }

    virtual void act() { m_fun(); }

};

template<typename F, typename... Args>
class ftor_behavior<false, true, F, Args...>  : public scheduled_actor
{

    static_assert(sizeof...(Args) > 0, "sizeof...(Args) == 0");

    F m_fun;
    tdata<Args...> m_args;

 public:

    ftor_behavior(F const& f, Args const&... args) : m_fun(f), m_args(args...)
    {
    }

    ftor_behavior(F&& f,Args const&... args) : m_fun(std::move(f))
                                             , m_args(args...)
    {
    }

    virtual void act() { invoke(m_fun, m_args); }

};

template<typename R>
scheduled_actor* get_behavior(std::integral_constant<bool,true>, R (*fptr)())
{
    static_assert(std::is_convertible<R, scheduled_actor*>::value == false,
                  "Spawning a function returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that function?");
    return new ftor_behavior<true, false, R (*)()>(fptr);
}

template<typename F>
scheduled_actor* get_behavior(std::integral_constant<bool,false>, F&& ftor)
{
    static_assert(std::is_convertible<decltype(ftor()), scheduled_actor*>::value == false,
                  "Spawning a functor returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that functor?");
    typedef typename util::rm_ref<F>::type ftype;
    return new ftor_behavior<false, false, ftype>(std::forward<F>(ftor));
}

template<typename F, typename Arg0, typename... Args>
scheduled_actor* get_behavior(std::integral_constant<bool,true>,
                             F fptr,
                             Arg0 const& arg0,
                             Args const&... args)
{
    static_assert(std::is_convertible<decltype(fptr(arg0, args...)), scheduled_actor*>::value == false,
                  "Spawning a function returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that function?");
    typedef ftor_behavior<true, true, F, Arg0, Args...> impl;
    return new impl(fptr, arg0, args...);
}

template<typename F, typename Arg0, typename... Args>
scheduled_actor* get_behavior(std::integral_constant<bool,false>,
                             F ftor,
                             Arg0 const& arg0,
                             Args const&... args)
{
    static_assert(std::is_convertible<decltype(ftor(arg0, args...)), scheduled_actor*>::value == false,
                  "Spawning a functor returning an actor_behavior? "
                  "Are you sure that you do not want to spawn the behavior "
                  "returned by that functor?");
    typedef typename util::rm_ref<F>::type ftype;
    typedef ftor_behavior<false, true, ftype, Arg0, Args...> impl;
    return new impl(std::forward<F>(ftor), arg0, args...);
}

} } // namespace cppa::detail

#endif // GET_BEHAVIOR_HPP
