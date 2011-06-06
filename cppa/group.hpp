#ifndef GROUP_HPP
#define GROUP_HPP

#include <string>
#include <memory>

#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/ref_counted.hpp"

namespace cppa {

class group : public channel
{

    std::string m_identifier;
    std::string m_module_name;

 protected:

    group(std::string&& id, std::string&& mod_name);

    group(const std::string& id, const std::string& mod_name);

    virtual void unsubscribe(const channel_ptr& who) = 0;

 public:

    class unsubscriber;

    friend class unsubscriber;

    // unsubscribes its channel from the group on destruction
    class unsubscriber : public attachable
    {

        friend class group;

        channel_ptr m_self;
        intrusive_ptr<group> m_group;

        unsubscriber() = delete;
        unsubscriber(const unsubscriber&) = delete;
        unsubscriber& operator=(const unsubscriber&) = delete;

     public:

        unsubscriber(const channel_ptr& s, const intrusive_ptr<group>& g);

        // matches on group
        bool matches(const attachable::token& what);

        virtual ~unsubscriber();

    };

    typedef std::unique_ptr<unsubscriber> subscription;

    class module
    {

     public:

        virtual const std::string& name() = 0;
        virtual intrusive_ptr<group> get(const std::string& group_name) = 0;

    };

    const std::string& identifier() const;

    const std::string& module_name() const;

    virtual subscription subscribe(const channel_ptr& who) = 0;

    static intrusive_ptr<group> get(const std::string& module_name,
                                    const std::string& group_name);

    static void add_module(module*);

};

typedef intrusive_ptr<group> group_ptr;

} // namespace cppa

#endif // GROUP_HPP
