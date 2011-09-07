#ifndef UNIFORM_TYPE_INFO_MAP_HPP
#define UNIFORM_TYPE_INFO_MAP_HPP

#include <set>
#include <string>
#include <utility> // std::pair

namespace cppa { class uniform_type_info; }

namespace cppa { namespace detail {

class uniform_type_info_map_helper;

// note: this class is implemented in uniform_type_info.cpp
class uniform_type_info_map
{

    friend class uniform_type_info_map_helper;

 public:

    typedef std::set<std::string> string_set;

    typedef std::map<std::string, uniform_type_info*> uti_map;

    typedef std::map< int, std::pair<string_set, string_set> > int_map;

    uniform_type_info_map();

    ~uniform_type_info_map();

    inline const int_map& int_names() const
    {
        return m_ints;
    }

    uniform_type_info* by_raw_name(const std::string& name) const;

    uniform_type_info* by_uniform_name(const std::string& name) const;

    std::vector<uniform_type_info*> get_all() const;

    // NOT thread safe!
    bool insert(const std::set<std::string>& raw_names, uniform_type_info* uti);

 private:

    // maps raw typeid names to uniform type informations
    uti_map m_by_rname;

    // maps uniform names to uniform type informations
    uti_map m_by_uname;

    // maps sizeof(-integer_type-) to { raw-names-set, uniform-names-set }
    int_map m_ints;

};

} } // namespace cppa::detail

#endif // UNIFORM_TYPE_INFO_MAP_HPP
