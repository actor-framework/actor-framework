// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/config_option.hpp"
#include "caf/detail/format.hpp"

using namespace caf;
using namespace std::literals;

struct person {
  std::string name;
  int age = 0;
};

CAF_BEGIN_TYPE_ID_BLOCK(driver, first_custom_type_id)
  CAF_ADD_TYPE_ID(driver, (person))
CAF_END_TYPE_ID_BLOCK(driver)

template <class Inspector>
auto inspect(Inspector& f, person& x) {
  return f.object(x).fields(f.field("name", x.name), f.field("age", x.age));
}

class config : public actor_system_config {
public:
  using super = actor_system_config;

  std::string some_string;
  int some_int = 0;
  std::vector<std::string> some_string_list;
  person some_person;
  std::vector<person> some_person_list;
  config() {
    // Global options.
    opt_group{custom_options_, "global"}
      .add(some_string, "some-string,s", "some string")
      .add(some_int, "some-int,i", "some integer")
      .add(some_string_list, "some-string-list,l", "some string list")
      .add(some_person, "some-person,p", "some person")
      .add(some_person_list, "some-person-list,P", "some person list");
    // Options for group "foo".
    opt_group{custom_options_, "foo"}
      .add<int>("bar,b", "some integer")
      .add<std::string>("baz,z", "some string");
    // Options for group "my-app" (my-app may be omitted from the command line).
    opt_group{custom_options_, "?my-app"}
      .add<int>("option-1,1,OPT1", "some integer")
      .add<std::string>("option-2,2", "some string");
  }

  settings dump_content() const override {
    auto result = super::dump_content();
    result.erase("caf");
    return result;
  }
};

struct out_t {};

out_t& operator<<(out_t& o, const std::string& str) {
  puts(str.c_str());
  return o;
}

int caf_main(actor_system&, const config& cfg) {
  out_t out{};
  out << "-- member variables --"s
      << detail::format(R"_(some-string = "{}")_", cfg.some_string)
      << detail::format(R"_(some-int = {})_", cfg.some_int)
      << detail::format(R"_(some-string-list = {})_",
                        caf::deep_to_string(cfg.some_string_list))
      << detail::format(R"_(some-person = {})_",
                        caf::deep_to_string(cfg.some_person))
      << detail::format(R"_(some-person-list = {})_",
                        caf::deep_to_string(cfg.some_person_list))
      << "-- config dump --"s;
  cfg.print_content();
  return 0;
}

CAF_MAIN(caf::id_block::driver)
