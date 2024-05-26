// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/pretty_type_name.hpp"

#include "caf/config.hpp"

#if defined(CAF_LINUX) || defined(CAF_MACOS) || defined(CAF_BSD)
#  define CAF_HAS_CXX_ABI
#endif

#ifdef CAF_HAS_CXX_ABI
#  include <cxxabi.h>
#  include <sys/types.h>
#  include <unistd.h>
#endif

#include "caf/string_algorithms.hpp"

namespace caf::detail {

void prettify_type_name(std::string& class_name) {
  // replace_all(class_name, " ", "");
  replace_all(class_name, "::", ".");
  replace_all(class_name, "(anonymous namespace)", "ANON");
  replace_all(class_name, ".__1.", "."); // gets rid of weird Clang-lib names
  replace_all(class_name, "class ", ".");
  replace_all(class_name, "struct ", ".");
  while (class_name[0] == '.' || class_name[0] == ' ')
    class_name.erase(class_name.begin());
  // Drop template parameters, only leaving the template class name.
  auto i = std::find(class_name.begin(), class_name.end(), '<');
  if (i != class_name.end())
    class_name.erase(i, class_name.end());
  // Finally, replace any whitespace with %20 (should never happen).
  replace_all(class_name, " ", "%20");
}

void prettify_type_name(std::string& class_name, const char* c_class_name) {
#ifdef CAF_HAS_CXX_ABI
  int stat = 0;
  std::unique_ptr<char, decltype(free)*> real_class_name{nullptr, free};
  auto tmp = abi::__cxa_demangle(c_class_name, nullptr, nullptr, &stat);
  real_class_name.reset(tmp);
  class_name = stat == 0 ? real_class_name.get() : c_class_name;
#else
  class_name = c_class_name;
#endif
  prettify_type_name(class_name);
}

std::string pretty_type_name(const std::type_info& x) {
  std::string result;
  prettify_type_name(result, x.name());
  return result;
}

} // namespace caf::detail
