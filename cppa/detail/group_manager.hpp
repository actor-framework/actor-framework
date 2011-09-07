#ifndef GROUP_MANAGER_HPP
#define GROUP_MANAGER_HPP

#include <map>

#include "cppa/group.hpp"
#include "cppa/detail/thread.hpp"
#include "cppa/util/shared_spinlock.hpp"

namespace cppa { namespace detail {

class group_manager
{

 public:

    group_manager();

    intrusive_ptr<group> get(const std::string& module_name,
                             const std::string& group_identifier);

    void add_module(group::module*);

 private:

    typedef std::map< std::string, std::unique_ptr<group::module> > modules_map;

    modules_map m_mmap;
    detail::mutex m_mmap_mtx;

};

} } // namespace cppa::detail

#endif // GROUP_MANAGER_HPP
