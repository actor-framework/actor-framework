/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_MIXIN_MEMORY_CACHED_HPP
#define CAF_MIXIN_MEMORY_CACHED_HPP

#include <utility>
#include <type_traits>

#include "caf/detail/memory.hpp"

namespace caf {
namespace mixin {

/**
 * This mixin adds all member functions and member variables needed
 * by the memory management subsystem.
 */
template <class Base, class Subtype>
class memory_cached : public Base {

  friend class detail::memory;

  template <class>
  friend class detail::basic_memory_cache;

 protected:

  using combined_type = memory_cached;

 public:

  static constexpr bool is_memory_cached_type = true;

  void request_deletion() override {
    auto mc = detail::memory::get_cache_map_entry(&typeid(*this));
    if (!mc) {
      auto om = outer_memory;
      if (om) {
        om->destroy();
        om->deallocate();
      } else
        delete this;
    } else
      mc->release_instance(mc->downcast(this));
  }

  template <class... Ts>
  memory_cached(Ts&&... args)
      : Base(std::forward<Ts>(args)...), outer_memory(nullptr) {}

 private:

  detail::instance_wrapper* outer_memory;

};

template <class T>
struct is_memory_cached {

  template <class U, bool = U::is_memory_cached_type>
  static std::true_type check(int);

  template <class>
  static std::false_type check(...);

 public:

  static constexpr bool value = decltype(check<T>(0))::value;

};

} // namespace mixin
} // namespace caf

#endif // CAF_MIXIN_MEMORY_CACHED_HPP
