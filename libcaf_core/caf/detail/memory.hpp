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

#ifndef CAF_DETAIL_MEMORY_HPP
#define CAF_DETAIL_MEMORY_HPP

#include <new>
#include <vector>
#include <memory>
#include <utility>
#include <typeinfo>

#include "caf/config.hpp"
#include "caf/ref_counted.hpp"

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

struct disposer {
  inline void operator()(memory_managed* ptr) const {
    ptr->request_deletion();
  }

};

class instance_wrapper {

 public:

  virtual ~instance_wrapper();

  // calls the destructor
  virtual void destroy() = 0;

  // releases memory
  virtual void deallocate() = 0;

};

class memory_cache {

 public:

  virtual ~memory_cache();

  // calls dtor and either releases memory or re-uses it later
  virtual void release_instance(void*) = 0;

  virtual std::pair<instance_wrapper*, void*> new_instance() = 0;

  // casts `ptr` to the derived type and returns it
  virtual void* downcast(memory_managed* ptr) = 0;

};

class instance_wrapper;

template <class T>
class basic_memory_cache;

#ifdef CAF_NO_MEM_MANAGEMENT

class memory {

  memory() = delete;

 public:

  // Allocates storage, initializes a new object, and returns the new instance.
  template <class T, class... Ts>
  static T* create(Ts&&... args) {
    return new T(std::forward<Ts>(args)...);
  }

  static inline memory_cache* get_cache_map_entry(const std::type_info*) {
    return nullptr;
  }

};

#else // CAF_NO_MEM_MANAGEMENT

template <class T>
class basic_memory_cache : public memory_cache {

  static constexpr size_t ne = s_alloc_size / sizeof(T);
  static constexpr size_t ms = ne < s_min_elements ? s_min_elements : ne;
  static constexpr size_t dsize = ms > s_max_elements ? s_max_elements : ms;

  struct wrapper : instance_wrapper {
    ref_counted* parent;
    union {
      T instance;

    };
    wrapper() : parent(nullptr) {}
    ~wrapper() {}
    void destroy() { instance.~T(); }
    void deallocate() { parent->deref(); }

  };

  class storage : public ref_counted {

   public:

    storage() {
      for (auto& elem : data) {
        // each instance has a reference to its parent
        elem.parent = this;
        ref(); // deref() is called in wrapper::deallocate
      }
    }

    using iterator = wrapper*;

    iterator begin() { return data; }

    iterator end() { return begin() + dsize; }

   private:

    wrapper data[dsize];

  };

 public:

  std::vector<wrapper*> cached_elements;

  basic_memory_cache() { cached_elements.reserve(dsize); }

  ~basic_memory_cache() {
    for (auto e : cached_elements) e->deallocate();
  }

  void* downcast(memory_managed* ptr) { return static_cast<T*>(ptr); }

  void release_instance(void* vptr) override {
    CAF_REQUIRE(vptr != nullptr);
    auto ptr = reinterpret_cast<T*>(vptr);
    CAF_REQUIRE(ptr->outer_memory != nullptr);
    auto wptr = static_cast<wrapper*>(ptr->outer_memory);
    wptr->destroy();
    wptr->deallocate();
  }

  std::pair<instance_wrapper*, void*> new_instance() override {
    if (cached_elements.empty()) {
      auto elements = new storage;
      for (auto i = elements->begin(); i != elements->end(); ++i) {
        cached_elements.push_back(i);
      }
    }
    wrapper* wptr = cached_elements.back();
    cached_elements.pop_back();
    return std::make_pair(wptr, &(wptr->instance));
  }

};

class memory {

  memory() = delete;

  template <class>
  friend class basic_memory_cache;

 public:

  // Allocates storage, initializes a new object, and returns the new instance.
  template <class T, class... Ts>
  static T* create(Ts&&... args) {
    auto mc = get_or_set_cache_map_entry<T>();
    auto p = mc->new_instance();
    auto result = new (p.second) T(std::forward<Ts>(args)...);
    result->outer_memory = p.first;
    return result;
  }

  static memory_cache* get_cache_map_entry(const std::type_info* tinf);

 private:

  static void add_cache_map_entry(const std::type_info* tinf,
                  memory_cache* instance);

  template <class T>
  static inline memory_cache* get_or_set_cache_map_entry() {
    auto mc = get_cache_map_entry(&typeid(T));
    if (!mc) {
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
