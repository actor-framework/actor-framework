/******************************************************************************\
 * This example is a simple dictionary implemented * using composable states. *
\******************************************************************************/

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 22-44 (Actor.tex)

#include <string>
#include <iostream>
#include <unordered_map>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using std::string;
using namespace caf;

namespace {

using dict = typed_actor<reacts_to<put_atom, string, string>,
                         replies_to<get_atom, string>::with<string>>;

class dict_behavior : public composable_behavior<dict> {
public:
  result<string> operator()(get_atom, param<string> key) override {
    auto i = values_.find(key);
    if (i == values_.end())
      return "";
    return i->second;
  }

  result<void> operator()(put_atom, param<string> key,
                          param<string> value) override {
    if (values_.count(key) != 0)
      return unit;
    values_.emplace(key.move(), value.move());
    return unit;
  }

protected:
  std::unordered_map<string, string> values_;
};

} // namespace

void caf_main(actor_system& system) {
  auto f = make_function_view(system.spawn<dict_behavior>());
  f(put_atom_v, "CAF", "success");
  cout << "CAF is the key to " << f(get_atom_v, "CAF") << endl;
}

CAF_MAIN()
