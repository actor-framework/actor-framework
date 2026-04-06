// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/build_config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/formatted.hpp"

#include <algorithm>
#include <string>
#include <typeinfo>

namespace caf::detail {

CAF_CORE_EXPORT std::string pretty_type_name(const char* class_name);

} // namespace caf::detail

#ifdef CAF_ENABLE_RTTI

#  include <typeinfo>

namespace caf::detail {

template <class T>
struct pretty_type_name_tag {};

template <class T>
struct simple_formatter<pretty_type_name_tag<T>> {
  template <class OutputIterator>
  OutputIterator format(pretty_type_name_tag<T>, OutputIterator out) const {
    auto name = pretty_type_name(typeid(T).name());
    return std::copy(name.begin(), name.end(), out);
  }
};

} // namespace caf::detail

#endif
