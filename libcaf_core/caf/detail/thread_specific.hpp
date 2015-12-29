/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#ifndef CAF_DETAIL_THREAD_SPECIFIC_HPP
#define CAF_DETAIL_THREAD_SPECIFIC_HPP

#include <cstddef>
#include <mutex>
#include <type_traits>
#include <utility>

#include "caf/config.hpp"

// defines `CAF_HAS_THREAD_LOCAL` if `thread_local` is supported
#if ! defined(CAF_CLANG) && ! defined(CAF_MACOS)
#  define CAF_HAS_THREAD_LOCAL
#else
#  include <pthread.h>
#endif

namespace caf {
namespace detail {

// wrapper of `T` that
// 1) calls `init` after object construction
// 2) calls `uninit` before object destruction
template <class T, class UninitFn>
class tls_obj_wrapper : public T {
public:
  // mostly to remove references
  using uninit_t = typename std::decay<UninitFn>::type;

  template <class InitFn, class Uninit, class... Args>
  tls_obj_wrapper(InitFn&& init, Uninit&& uninit, Args&&... args)
      : T{ std::forward<Args>(args)... }
      , uninit_{ std::forward<Uninit>(uninit) } {
    // member functions may be `&&` suffixed
    if (std::forward<InitFn>(init)) {
      // makes no assumption on what parameter type `init` takes
      // hides implementation details; appears as if `T`
      T& tref = *this;
      std::forward<InitFn>(init)(tref);
    }
  }

  ~tls_obj_wrapper() {
    if (uninit_) {
      // makes no assumption on what parameter type `uninit_` takes
      // hides implementation details; appears as if `T`
      T& tref = *this;
      uninit_(tref);
    }
  }

  // only used in pthreads implementation
  static void destroy(void* ptr) {
    auto tls_obj = static_cast<tls_obj_wrapper*>(ptr);
    delete tls_obj;
  }

private:
  uninit_t uninit_;
};

template <class T, class Fn>
auto forward_tls_fn(Fn&& fn) -> decltype(std::forward<Fn>(fn)) {
  return std::forward<Fn>(fn);
}

// enables passing `nullptr` as init/uninit routine
template <class T>
void(* forward_tls_fn(std::nullptr_t) )(T&) {
  return nullptr;
}

#if defined(CAF_HAS_THREAD_LOCAL)
// `thread_local` implementation

template <class T, class Tag, class InitFn, class UninitFn, class... Args>
T& thread_specific_impl(InitFn&& init, UninitFn&& uninit, Args&&... args) {
  using impl_t = tls_obj_wrapper<T, UninitFn>;
  thread_local impl_t tls_obj{ std::forward<InitFn>(init),
                               std::forward<UninitFn>(uninit),
                               std::forward<Args>(args)... };
  return tls_obj;
}

#else // ! CAF_HAS_THREAD_LOCAL
// pthreads implementation

template <class T, class Tag, class InitFn, class UninitFn, class... Args>
T& thread_specific_impl(InitFn&& init, UninitFn&& uninit, Args&&... args) {
  using impl_t = tls_obj_wrapper<T, UninitFn>;
  static std::once_flag once;
  static pthread_key_t key{};
  auto key_ptr = &key;
  std::call_once(once, [key_ptr] {
    pthread_key_create(key_ptr, impl_t::destroy);
  });
  auto tls_obj = static_cast<impl_t*>(pthread_getspecific(key));
  if (! tls_obj) {
    tls_obj = new impl_t{ std::forward<InitFn>(init),
                          std::forward<UninitFn>(uninit),
                          std::forward<Args>(args)... };
    pthread_setspecific(key, tls_obj);
  }
  return *tls_obj;
}

#endif

// provides portable interface to thread local storage (TLS)
// uses `thread_local` if supported, and pthreads otherwise
// `T`     : type of thread local object
// `Tag`   : used for identification purpose; may be atom types
//           typically, you don't need to bother with this
//           other template arguments are just sufficient
// `init`  : called after object construction
// `uninit`: called before object destruction
// `args`  : constructor arguments
// note: theoretically, there is no need for two overloads
// but alas, see CWG issue 1609 (http://wg21.link/CWG1609)

// overload for empty `args`
template <class T, class Tag = void,
          class InitFn = void(*)(T&),
          class UninitFn = void(*)(T&)>
T& thread_specific(InitFn&& init = InitFn{},
                   UninitFn&& uninit = UninitFn{}) {
  return thread_specific_impl<T, Tag>(
    forward_tls_fn<T>(std::forward<InitFn>(init)),
    forward_tls_fn<T>(std::forward<UninitFn>(uninit)));
}

// overload for non-empty `args`
template <class T, class Tag = void,
          class InitFn,
          class UninitFn,
          class... Args>
T& thread_specific(InitFn&& init,
                   UninitFn&& uninit,
                   Args&&... args) {
  return thread_specific_impl<T, Tag>(
    forward_tls_fn<T>(std::forward<InitFn>(init)),
    forward_tls_fn<T>(std::forward<UninitFn>(uninit)),
    std::forward<Args>(args)...);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_THREAD_SPECIFIC_HPP
