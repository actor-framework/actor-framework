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

#ifndef CAF_DETAIL_MEMORY_HPP
#define CAF_DETAIL_MEMORY_HPP

#include <new>
#include <vector>
#include <memory>
#include <utility>
#include <typeinfo>

#include "caf/config.hpp"
#include "caf/ref_counted.hpp"

#include "caf/detail/embedded.hpp"
#include "caf/detail/memory_cache_flag_type.hpp"

namespace caf {
class mailbox_element;
} // namespace caf

namespace caf {
namespace detail {

namespace {

constexpr size_t s_alloc_size = 1024 * 1024;    // allocate ~1mb chunks
constexpr size_t s_cache_size = 10 * 1024 * 1024; // cache about 10mb per thread
constexpr size_t s_min_elements = 5;        // don't create < 5 elements
constexpr size_t s_max_elements = 20;       // don't create > 20 elements

} // namespace <anonymous>

using embedded_storage = std::pair<intrusive_ptr<ref_counted>, void*>;

class memory_cache {
public:
  virtual ~memory_cache();
  virtual embedded_storage new_embedded_storage() = 0;
};

template <class T>
class basic_memory_cache;

#ifdef CAF_NO_MEM_MANAGEMENT

template <class T>
struct rc_storage : public ref_counted {
  T instance;
  template <class... Ts>
  rc_storage(Ts&&... xs)
      : instance(intrusive_ptr<ref_counted>(this, false),
        std::forward<Ts>(xs)...) {
    CAF_ASSERT(get_reference_count() >= 1);
  }
};

template <class T>
T* unbox_rc_storage(T* ptr) {
  return ptr;
}

template <class T>
T* unbox_rc_storage(rc_storage<T>* ptr) {
  return &(ptr->instance);
}

class memory {
public:
  memory() = delete;

  // Allocates storage, initializes a new object, and returns the new instance.
  template <class T, class... Ts>
  static T* create(Ts&&... xs) {
    using embedded_t =
      typename std::conditional<
        T::memory_cache_flag == provides_embedding,
        rc_storage<T>,
        T
       >::type;
    return unbox_rc_storage(new embedded_t(std::forward<Ts>(xs)...));
  }

  static inline memory_cache* get_cache_map_entry(const std::type_info*) {
    return nullptr;
  }
};

#else // CAF_NO_MEM_MANAGEMENT

template <class T>
class basic_memory_cache : public memory_cache {
public:
  static constexpr size_t ne = s_alloc_size / sizeof(T);
  static constexpr size_t ms = ne < s_min_elements ? s_min_elements : ne;
  static constexpr size_t dsize = ms > s_max_elements ? s_max_elements : ms;

  static_assert(dsize > 0, "dsize == 0");

  using embedded_t =
    typename std::conditional<
      T::memory_cache_flag == needs_embedding,
      embedded<T>,
      T
     >::type;

  struct wrapper {
    union {
      embedded_t instance;
    };

    wrapper() {
      // nop
    }

    ~wrapper() {
      // nop
    }
  };

  class storage : public ref_counted {
  public:
    storage() : pos_(0) {
      // nop
    }

    ~storage() {
      // nop
    }

    bool has_next() {
      return pos_ < dsize;
    }

    embedded_t* next() {
      return &(data_[pos_++].instance);
    }

  private:
    size_t pos_;
    wrapper data_[dsize];
  };

  embedded_storage new_embedded_storage() override {
    // allocate cache on-the-fly
    if (! cache_) {
      cache_.reset(new storage, false); // starts with ref count of 1
      CAF_ASSERT(cache_->unique());
    }
    auto res = cache_->next();
    if (cache_->has_next()) {
      return {cache_, res};
    }
    // we got the last element out of the cache; pass the reference to the
    // client to avoid pointless increase/decrease ops on the reference count
    embedded_storage result;
    result.first.reset(cache_.release(), false);
    result.second = res;
    return result;
  }

private:
  intrusive_ptr<storage> cache_;
};

class memory {

  memory() = delete;

  template <class>
  friend class basic_memory_cache;

public:

  // Allocates storage, initializes a new object, and returns the new instance.
  template <class T, class... Ts>
  static T* create(Ts&&... xs) {
    using embedded_t =
      typename std::conditional<
        T::memory_cache_flag == needs_embedding,
        embedded<T>,
        T
       >::type;
    auto mc = get_or_set_cache_map_entry<T>();
    auto es = mc->new_embedded_storage();
    auto ptr = reinterpret_cast<embedded_t*>(es.second);
    new (ptr) embedded_t(std::move(es.first), std::forward<Ts>(xs)...);
    return ptr;
  }

  static memory_cache* get_cache_map_entry(const std::type_info* tinf);

private:

  static void add_cache_map_entry(const std::type_info* tinf,
                                  memory_cache* instance);

  template <class T>
  static inline memory_cache* get_or_set_cache_map_entry() {
    auto mc = get_cache_map_entry(&typeid(T));
    if (! mc) {
      mc = new basic_memory_cache<T>;
      add_cache_map_entry(&typeid(T), mc);
    }
    return mc;
  }

};

#endif // CAF_NO_MEM_MANAGEMENT

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_MEMORY_HPP
