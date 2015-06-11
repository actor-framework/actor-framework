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

#include "caf/detail/memory.hpp"

#include <vector>
#include <typeinfo>

#include "caf/mailbox_element.hpp"

#ifdef CAF_NO_MEM_MANAGEMENT

int caf_memory_keep_compiler_happy() {
  // this function shuts up a linker warning saying that the
  // object file has no symbols
  return 0;
}

#else // CAF_NO_MEM_MANAGEMENT

namespace caf {
namespace detail {

namespace {

pthread_key_t s_key;
pthread_once_t s_key_once = PTHREAD_ONCE_INIT;

} // namespace <anonymous>

memory_cache::~memory_cache() {
  // nop
}

using cache_map = std::map<const std::type_info*,std::unique_ptr<memory_cache>>;

void cache_map_destructor(void* ptr) {
  delete reinterpret_cast<cache_map*>(ptr);
}

void make_cache_map() {
  pthread_key_create(&s_key, cache_map_destructor);
}

cache_map& get_cache_map() {
  pthread_once(&s_key_once, make_cache_map);
  auto cache = reinterpret_cast<cache_map*>(pthread_getspecific(s_key));
  if (! cache) {
    cache = new cache_map;
    pthread_setspecific(s_key, cache);
    // insert default types
    std::unique_ptr<memory_cache> tmp(new basic_memory_cache<mailbox_element>);
    cache->emplace(&typeid(mailbox_element), move(tmp));
  }
  return *cache;
}

memory_cache* memory::get_cache_map_entry(const std::type_info* tinf) {
  auto& cache = get_cache_map();
  auto i = cache.find(tinf);
  if (i != cache.end()) {
    return i->second.get();
  }
  return nullptr;
}

void memory::add_cache_map_entry(const std::type_info* tinf,
                                 memory_cache* instance) {
  auto& cache = get_cache_map();
  cache[tinf].reset(instance);
}

} // namespace detail
} // namespace caf

#endif // CAF_NO_MEM_MANAGEMENT
