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

#ifndef CAF_ABSTRACT_TUPLE_HPP
#define CAF_ABSTRACT_TUPLE_HPP

#include <string>
#include <iterator>
#include <typeinfo>

#include "caf/intrusive_ptr.hpp"

#include "caf/config.hpp"
#include "caf/ref_counted.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/type_list.hpp"
#include "caf/detail/message_iterator.hpp"

namespace caf {
namespace detail {

class message_data : public ref_counted {

  using super = ref_counted;

 public:

  message_data();

  // mutators
  virtual void* mutable_at(size_t pos) = 0;
  virtual void* mutable_native_data();

  // accessors
  virtual size_t size() const = 0;

  virtual message_data* copy() const = 0;

  virtual const void* at(size_t pos) const = 0;

  virtual const std::string* tuple_type_names() const = 0;

  /**
   * Tries to match element at position `pos` to given RTTI.
   * @param pos Index of element in question.
   * @param typenr Number of queried type or `0` for custom types.
   * @param rtti Queried type or `nullptr` for builtin types.
   */
  virtual bool match_element(size_t pos, uint16_t typenr,
                             const std::type_info* rtti) const = 0;

  virtual uint32_t type_token() const = 0;

  // returns either tdata<...> object or nullptr (default) if tuple
  // is not a 'native' implementation
  virtual const void* native_data() const;

  virtual const char* uniform_name_at(size_t pos) const = 0;

  virtual uint16_t type_nr_at(size_t pos) const = 0;

  bool equals(const message_data& other) const;

  using const_iterator = message_iterator;

  inline const_iterator begin() const {
    return {this};
  }
  inline const_iterator cbegin() const {
    return {this};
  }

  inline const_iterator end() const {
    return {this, size()};
  }
  inline const_iterator cend() const {
    return {this, size()};
  }

  class ptr {

   public:

    ptr() = default;
    ptr(ptr&&) = default;
    ptr(const ptr&) = default;
    ptr& operator=(ptr&&) = default;
    ptr& operator=(const ptr&) = default;

    inline explicit ptr(message_data* p) : m_ptr(p) {}

    inline void detach() { static_cast<void>(get_detached()); }

    inline message_data* operator->() { return get_detached(); }
    inline message_data& operator*() { return *get_detached(); }
    inline const message_data* operator->() const { return m_ptr.get(); }
    inline const message_data& operator*() const { return *m_ptr.get(); }
    inline void swap(ptr& other) { m_ptr.swap(other.m_ptr); }
    inline void reset(message_data* p = nullptr) { m_ptr.reset(p); }

    inline explicit operator bool() const {
      return static_cast<bool>(m_ptr);
    }

    inline message_data* get() const {
      return m_ptr.get();
    }

   private:

    message_data* get_detached();

    intrusive_ptr<message_data> m_ptr;

  };
};

std::string get_tuple_type_names(const detail::message_data&);

} // namespace detail
} // namespace caf

#endif // CAF_ABSTRACT_TUPLE_HPP
