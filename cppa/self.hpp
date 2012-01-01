#ifndef SELF_HPP
#define SELF_HPP

#include "cppa/actor.hpp"
#include "cppa/local_actor.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

#ifdef CPPA_DOCUMENTATION

/**
 * @brief Always points to the current actor. Similar to @c this in
 *        an object-oriented context.
 */
extern local_actor* self;

#else

// convertible<...> enables "actor_ptr this_actor = self;"
class self_type : public convertible<self_type, actor*>
{

    static void set_impl(local_actor*);

    static local_actor* get_unchecked_impl();

    static local_actor* get_impl();

    self_type(self_type const&) = delete;
    self_type& operator=(self_type const&) = delete;

 public:

    constexpr self_type() { }

    // "inherited" from convertible<...>
    inline local_actor* do_convert() const
    {
        return get_impl();
    }

    // allow "self" wherever an local_actor or actor pointer is expected
    inline operator local_actor*() const
    {
        return get_impl();
    }

    inline local_actor* operator->() const
    {
        return get_impl();
    }

    // backward compatibility
    inline local_actor* operator()() const
    {
        return get_impl();
    }

    // @pre get_unchecked() == nullptr
    inline void set(local_actor* ptr) const
    {
        set_impl(ptr);
    }

    // @returns The current value without converting the calling context
    //          to an actor on-the-fly.
    inline local_actor* unchecked() const
    {
        return get_unchecked_impl();
    }

};

/*
 * "self" emulates a new keyword. The object itself is useless, all it does
 * is to provide syntactic sugar like "self->trap_exit(true)".
 */
constexpr self_type self;

#endif

} // namespace cppa

#endif // SELF_HPP
