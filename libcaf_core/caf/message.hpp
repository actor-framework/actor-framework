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

#ifndef CAF_MESSAGE_HPP
#define CAF_MESSAGE_HPP

#include <tuple>
#include <type_traits>

#include "caf/atom.hpp"
#include "caf/config.hpp"
#include "caf/from_string.hpp"
#include "caf/skip_message.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"

#include "caf/detail/tuple_vals.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {

class message_handler;

/**
 * Describes a fixed-length copy-on-write tuple with elements of any type.
 */
class message {

 public:

  /**
   * A raw pointer to the data.
   */
  using raw_ptr = detail::message_data*;

  /**
   * A (COW) smart pointer to the data.
   */
  using data_ptr = detail::message_data::ptr;

  /**
   * Creates an empty tuple.
   */
  message() = default;

  /**
   * Move constructor.
   */
  message(message&&);

  /**
   * Copy constructor.
   */
  message(const message&) = default;

  /**
   * Move assignment.
   */
  message& operator=(message&&);

  /**
   * Copy assignment.
   */
  message& operator=(const message&) = default;

  /**
   * Gets the size of this tuple.
   */
  inline size_t size() const {
    return m_vals ? m_vals->size() : 0;
  }

  /**
   * Creates a new message with all but the first n values.
   */
  message drop(size_t n) const;

  /**
   * Creates a new message with all but the last n values.
   */
  message drop_right(size_t n) const;

  /**
   * Creates a new message from the first n values.
   */
  inline message take(size_t n) const {
    return n >= size() ? *this : drop_right(size() - n);
  }

  /**
   * Creates a new message from the last n values.
   */
  inline message take_right(size_t n) const {
    return n >= size() ? *this : drop(size() - n);
  }

  /**
   * Creates a new message of size `n` starting at element `pos`.
   */
  message slice(size_t pos, size_t n) const;

  /**
   * Gets a mutable pointer to the element at position @p p.
   */
  void* mutable_at(size_t p);

  /**
   * Gets a const pointer to the element at position @p p.
   */
  const void* at(size_t p) const;

  const char* uniform_name_at(size_t pos) const;

  /**
   * Returns @c true if `*this == other, otherwise false.
   */
  bool equals(const message& other) const;

  /**
   * Returns true if `size() == 0, otherwise false.
   */
  inline bool empty() const {
    return size() == 0;
  }

  /**
   * Returns the value at @p as instance of @p T.
   */
  template <class T>
  inline const T& get_as(size_t p) const {
    CAF_REQUIRE(match_element(p, detail::type_nr<T>::value, &typeid(T)));
    return *reinterpret_cast<const T*>(at(p));
  }

  /**
   * Returns the value at @p as mutable data_ptr& of type @p T&.
   */
  template <class T>
  inline T& get_as_mutable(size_t p) {
    CAF_REQUIRE(match_element(p, detail::type_nr<T>::value, &typeid(T)));
    return *reinterpret_cast<T*>(mutable_at(p));
  }

  /**
   * Returns a copy-on-write pointer to the internal data.
   */
  inline data_ptr& vals() {
    return m_vals;
  }

  /**
   * Returns a const copy-on-write pointer to the internal data.
   */
  inline const data_ptr& vals() const {
    return m_vals;
  }

  /**
   * Returns a const copy-on-write pointer to the internal data.
   */
  inline const data_ptr& cvals() const {
    return m_vals;
  }

  /**
   * Checks whether this message is dynamically typed, i.e.,
   * its types were not known at compile time.
   */
  bool dynamically_typed() const;

  /**
   * Applies `handler` to this message and returns the result
   *  of `handler(*this)`.
   */
  optional<message> apply(message_handler handler);

  /**
   * Returns a new message consisting of all elements that were *not*
   * matched by `handler`.
   */
  message filter(message_handler handler) const;

  /**
   * Stores the name of a command line option ("<long name>[,<short name>]")
   * along with a description and a callback.
   */
  struct cli_arg {
    std::string name;
    std::string text;
    std::function<bool (const std::string&)> fun;
    inline cli_arg(std::string nstr, std::string tstr)
        : name(std::move(nstr)),
          text(std::move(tstr)) {
      // nop
    }
    inline cli_arg(std::string nstr, std::string tstr, std::string& arg)
        : name(std::move(nstr)),
          text(std::move(tstr)) {
      fun = [&arg](const std::string& str) -> bool{
        arg = str;
        return true;
      };
    }
    inline cli_arg(std::string nstr, std::string tstr, std::vector<std::string>& arg)
        : name(std::move(nstr)),
          text(std::move(tstr)) {
      fun = [&arg](const std::string& str) -> bool {
        arg.push_back(str);
        return true;
      };
    }
    template <class T>
    cli_arg(std::string nstr, std::string tstr, T& arg)
        : name(std::move(nstr)),
          text(std::move(tstr)) {
      fun = [&arg](const std::string& str) -> bool {
        auto res = from_string<T>(str);
        if (!res) {
          return false;
        }
        arg = *res;
        return true;
      };
    }
    template <class T>
    cli_arg(std::string nstr, std::string tstr, std::vector<T>& arg)
        : name(std::move(nstr)),
          text(std::move(tstr)) {
      fun = [&arg](const std::string& str) -> bool {
        auto res = from_string<T>(str);
        if (!res) {
          return false;
        }
        arg.push_back(*res);
        return true;
      };
    }
  };

  struct cli_res;

  /**
   * A simplistic interface for using `filter` to parse command line options.
   */
  cli_res filter_cli(std::vector<cli_arg> args) const;

  /**
   * Queries whether the element at `pos` is of type `T`.
   * @param pos Index of element in question.
   */
  template <class T>
  bool match_element(size_t pos) const {
    const std::type_info* rtti = nullptr;
    if (detail::type_nr<T>::value == 0) {
      rtti = &typeid(T);
    }
    return match_element(pos, detail::type_nr<T>::value, rtti);
  }

  /**
   * Queries whether the types of this message are `Ts...`.
   */
  template <class... Ts>
  bool match_elements() const {
    std::integral_constant<size_t, 0> p0;
    detail::type_list<Ts...> tlist;
    return size() == sizeof...(Ts) && match_elements_impl(p0, tlist);
  }


  /** @cond PRIVATE */

  inline uint32_t type_token() const {
    return m_vals ? m_vals->type_token() : 0xFFFFFFFF;
  }

  inline void force_detach() {
    m_vals.detach();
  }

  void reset(raw_ptr new_ptr = nullptr);

  void swap(message& other);

  explicit message(raw_ptr);

  inline std::string tuple_type_names() const {
    return m_vals->tuple_type_names();
  }

  explicit message(const data_ptr& vals);

  struct move_from_tuple_helper {
    template <class... Ts>
    inline message operator()(Ts&... vs) {
      return make_message(std::move(vs)...);
    }
  };

  template <class... Ts>
  inline message move_from_tuple(std::tuple<Ts...>&& tup) {
    move_from_tuple_helper f;
    return detail::apply_args(f, detail::get_indices(tup), tup);
  }

  /**
   * Tries to match element at position `pos` to given RTTI.
   * @param pos Index of element in question.
   * @param typenr Number of queried type or `0` for custom types.
   * @param rtti Queried type or `nullptr` for builtin types.
   */
  bool match_element(size_t pos, uint16_t typenr,
                     const std::type_info* rtti) const;

  template <class T, class... Ts>
  bool match_elements(detail::type_list<T, Ts...> list) const {
    std::integral_constant<size_t, 0> p0;
    return size() == (sizeof...(Ts) + 1) && match_elements_impl(p0, list);
  }

  /** @endcond */

 private:
  template <size_t P>
  bool match_elements_impl(std::integral_constant<size_t, P>,
                           detail::type_list<>) const {
    return true; // end of recursion
  }
  template <size_t P, class T, class... Ts>
  bool match_elements_impl(std::integral_constant<size_t, P>,
                           detail::type_list<T, Ts...>) const {
    std::integral_constant<size_t, P + 1> next_p;
    detail::type_list<Ts...> next_list;
    return match_element<T>(P)
        && match_elements_impl(next_p, next_list);
  }
  message filter_impl(size_t start, message_handler handler) const;
  data_ptr m_vals;
};

/**
 * Stores the result of `message::filter_cli`.
 */
struct message::cli_res {
  /**
   * Stores the remaining (unmatched) arguments.
   */
  message remainder;
  /**
   * Stores the names of all active options.
   */
  std::set<std::string> opts;
  /**
   * Stores the automatically generated help text.
   */
  std::string helptext;
};

/**
 * @relates message
 */
inline bool operator==(const message& lhs, const message& rhs) {
  return lhs.equals(rhs);
}

/**
 * @relates message
 */
inline bool operator!=(const message& lhs, const message& rhs) {
  return !(lhs == rhs);
}

template <class T>
struct unbox_message_element {
  using type = T;
};

template <atom_value V>
struct unbox_message_element<atom_constant<V>> {
  using type = atom_value;
};

/**
 * Creates a new `message` containing the elements `args...`.
 * @relates message
 */
template <class T, class... Ts>
typename std::enable_if<
  !std::is_same<message, typename std::decay<T>::type>::value
  || (sizeof...(Ts) > 0),
  message
>::type
make_message(T&& arg, Ts&&... args) {
  using storage
    = detail::tuple_vals<typename unbox_message_element<
                           typename detail::strip_and_convert<T>::type
                         >::type,
                         typename unbox_message_element<
                           typename detail::strip_and_convert<Ts>::type
                         >::type...>;
  auto ptr = new storage(std::forward<T>(arg), std::forward<Ts>(args)...);
  return message{detail::message_data::ptr{ptr}};
}

/**
 * Returns a copy of @p other.
 * @relates message
 */
inline message make_message(message other) {
  return std::move(other);
}

} // namespace caf

#endif // CAF_MESSAGE_HPP
