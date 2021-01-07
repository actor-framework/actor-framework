// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/memory_managed.hpp"

namespace caf::detail {

class disposer {
public:
  void operator()(memory_managed* ptr) const noexcept {
    ptr->request_deletion(false);
  }

  template <class T>
  typename std::enable_if<!std::is_base_of<memory_managed, T>::value>::type
  operator()(T* ptr) const noexcept {
    delete ptr;
  }
};

} // namespace caf::detail
