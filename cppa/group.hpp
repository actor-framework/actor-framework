#ifndef GROUP_HPP
#define GROUP_HPP

#include <string>

#include "cppa/channel.hpp"
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

    class subscription;

    friend class subscription;

    class subscription
    {

        channel_ptr m_self;
        intrusive_ptr<group> m_group;

        subscription() = delete;
        subscription(const subscription&) = delete;
        subscription& operator=(const subscription&) = delete;

     public:

        inline explicit operator bool()
        {
            return m_self != nullptr && m_group != nullptr;
        }

        subscription(const channel_ptr& s, const intrusive_ptr<group>& g);
        subscription(subscription&& other);
        ~subscription();

    };

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
