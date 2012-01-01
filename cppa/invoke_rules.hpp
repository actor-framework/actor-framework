#ifndef INVOKE_RULES_HPP
#define INVOKE_RULES_HPP

#include <list>
#include <memory>

#include "cppa/detail/invokable.hpp"
#include "cppa/detail/intermediate.hpp"

namespace cppa { namespace detail {

/*
class invokable;
class intermediate;
class timed_invokable;
*/

typedef std::unique_ptr<detail::invokable> invokable_ptr;
typedef std::unique_ptr<detail::timed_invokable> timed_invokable_ptr;

} } // namespace cppa::detail

namespace cppa { namespace util { class duration; } }

namespace cppa {

class any_tuple;
class invoke_rules;
class timed_invoke_rules;

typedef std::list<detail::invokable_ptr> invokable_list;

/**
 * @brief Base of {@link timed_invoke_rules} and {@link invoke_rules}.
 */
class invoke_rules_base
{

    friend class invoke_rules;
    friend class timed_invoke_rules;

 private:

    invoke_rules_base() = default;

    invoke_rules_base(invokable_list&& ilist);

 protected:

    invokable_list m_list;

 public:

    virtual ~invoke_rules_base();

    /**
     * @brief Tries to match @p data with one of the stored patterns.
     *
     * If a pattern matched @p data, the corresponding callback is invoked.
     * @param data Data tuple that should be matched.
     * @returns @p true if a pattern matched @p data;
     *          otherwise @p false.
     */
    bool operator()(any_tuple const& data) const;

    /**
     * @brief Tries to match @p data with one of the stored patterns.
     * @param data Data tuple that should be matched.
     * @returns An {@link intermediate} instance that could invoke
     *          the corresponding callback; otherwise @p nullptr.
     */
    detail::intermediate* get_intermediate(any_tuple const& data) const;

};

/**
 * @brief Invoke rules with timeout.
 */
class timed_invoke_rules : public invoke_rules_base
{

    typedef invoke_rules_base super;

    friend class invoke_rules;

    timed_invoke_rules(timed_invoke_rules const&) = delete;
    timed_invoke_rules& operator=(timed_invoke_rules const&) = delete;

    timed_invoke_rules(invokable_list&& prepended_list,
                       timed_invoke_rules&& other);

    static util::duration default_timeout;

public:

    timed_invoke_rules();
    timed_invoke_rules(timed_invoke_rules&& arg);
    timed_invoke_rules(detail::timed_invokable_ptr&& arg);

    timed_invoke_rules& operator=(timed_invoke_rules&&);

    util::duration const& timeout() const;

    void handle_timeout() const;

 private:

    detail::timed_invokable_ptr m_ti;

};

/**
 * @brief Invoke rules without timeout.
 */
class invoke_rules : public invoke_rules_base
{

    typedef invoke_rules_base super;

    friend class timed_invoke_rules;

    invoke_rules(invoke_rules const&) = delete;
    invoke_rules& operator=(invoke_rules const&) = delete;

 public:

    invoke_rules() = default;

    invoke_rules(invoke_rules&& other);

    invoke_rules(std::unique_ptr<detail::invokable>&& arg);

    invoke_rules& splice(invoke_rules&& other);

    timed_invoke_rules splice(timed_invoke_rules&& other);

    invoke_rules operator,(invoke_rules&& other);

    timed_invoke_rules operator,(timed_invoke_rules&& other);

    invoke_rules& operator=(invoke_rules&& other);

 private:

    invoke_rules(invokable_list&& ll);

    invoke_rules& splice(invokable_list&& ilist);

};

} // namespace cppa

#endif // INVOKE_RULES_HPP
