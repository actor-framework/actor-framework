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

#ifndef CAF_MESSAGE_HPP
#define CAF_MESSAGE_HPP

#include <tuple>
#include <sstream>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/skip.hpp"
#include "caf/atom.hpp"
#include "caf/config.hpp"
#include "caf/optional.hpp"
#include "caf/make_counted.hpp"
#include "caf/index_mapping.hpp"
#include "caf/allowed_unsafe_message_type.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {
class message_handler;

/// Describes a fixed-length, copy-on-write, type-erased
/// tuple with elements of any type.
class message {
public:
  // -- nested types -----------------------------------------------------------

  struct cli_arg;

  struct cli_res;

  // -- member types -----------------------------------------------------------

  /// Raw pointer to content.
  using raw_ptr = detail::message_data*;

  /// Copy-on-write pointer to content.
  using data_ptr = detail::message_data::cow_ptr;

  /// Function object for generating CLI argument help text.
  using help_factory = std::function<std::string (const std::vector<cli_arg>&)>;

  // -- constructors, destructors, and assignment operators --------------------

  message() noexcept = default;
  message(const message&) noexcept = default;
  message& operator=(const message&) noexcept = default;

  message(message&&) noexcept;
  message& operator=(message&&) noexcept;
  explicit message(const data_ptr& vals) noexcept;

  ~message();

  // -- factories --------------------------------------------------------------

  /// Creates a new message by concatenating `xs...`.
  template <class... Ts>
  static message concat(const Ts&... xs) {
    return concat_impl({xs.vals()...});
  }

  /// Creates a new message by copying all elements in a type-erased tuple.
  static message copy(const type_erased_tuple& xs);

  // -- modifiers --------------------------------------------------------------

  /// Concatenates `*this` and `x`.
  message& operator+=(const message& x);

  /// Returns a mutable pointer to the element at position `p`.
  void* get_mutable(size_t p);

  /// Returns the value at position `p` as mutable reference of type `T`.
  template <class T>
  T& get_mutable_as(size_t p) {
    CAF_ASSERT(match_element(p, type_nr<T>::value, &typeid(T)));
    return *reinterpret_cast<T*>(get_mutable(p));
  }

  /// Returns `handler(*this)`.
  optional<message> apply(message_handler handler);

  /// Forces the message to copy its content if there are more than
  /// one references to the content.
  inline void force_unshare() {
    vals_.unshare();
  }

  /// Returns a mutable reference to the content. Callers are responsible
  /// for unsharing content if necessary.
  inline data_ptr& vals() {
    return vals_;
  }

  /// Exchanges content of `this` and `other`.
  void swap(message& other) noexcept;

  /// Assigns new content.
  void reset(raw_ptr new_ptr = nullptr, bool add_ref = true) noexcept;

  // -- observers --------------------------------------------------------------

  /// Creates a new message with all but the first n values.
  message drop(size_t n) const;

  /// Creates a new message with all but the last n values.
  message drop_right(size_t n) const;

  /// Creates a new message of size `n` starting at the element at position `p`.
  message slice(size_t p, size_t n) const;

  /// Filters this message by applying slices of it to `handler` and  returns
  /// the remaining elements of this operation. Slices are generated in the
  /// sequence `[0, size)`, `[0, size-1)`, `...` , `[1, size-1)`, `...`,
  /// `[size-1, size)`. Whenever a slice matches, it is removed from the message
  /// and the next slice starts at the *same* index on the reduced message.
  ///
  /// For example:
  ///
  /// ~~~
  /// auto msg = make_message(1, 2.f, 3.f, 4);
  /// // extract float and integer pairs
  /// auto msg2 = msg.extract({
  ///   [](float, float) { },
  ///   [](int, int) { }
  /// });
  /// assert(msg2 == make_message(1, 4));
  /// ~~~
  ///
  /// Step-by-step explanation:
  /// - Slice 1: `(1, 2.f, 3.f, 4)`, no match
  /// - Slice 2: `(1, 2.f, 3.f)`, no match
  /// - Slice 3: `(1, 2.f)`, no match
  /// - Slice 4: `(1)`, no match
  /// - Slice 5: `(2.f, 3.f, 4)`, no match
  /// - Slice 6: `(2.f, 3.f)`, *match*; new message is `(1, 4)`
  /// - Slice 7: `(4)`, no match
  ///
  /// Slice 7 is `(4)`, i.e., does not contain the first element, because the
  /// match on slice 6 occurred at index position 1. The function `extract`
  /// iterates a message only once, from left to right.
  message extract(message_handler handler) const;

  /// A simplistic interface for using `extract` to parse command line options.
  /// Usage example:
  ///
  /// ~~~
  /// int main(int argc, char** argv) {
  ///   uint16_t port;
  ///   string host = "localhost";
  ///   auto res = message_builder(argv + 1, argv + argc).extract_opts({
  ///     {"port,p", "set port", port},
  ///     {"host,H", "set host (default: localhost)", host},
  ///     {"verbose,v", "enable verbose mode"}
  ///   });
  ///   if (!res.error.empty()) {
  ///     cerr << res.error << endl;
  ///     return 1;
  ///   }
  ///   if (res.opts.count("help") > 0) {
  ///     // CLI arguments contained "-h", "--help", or "-?" (builtin);
  ///     cout << res.helptext << endl;
  ///     return 0;
  ///   }
  ///   if (!res.remainder.empty()) {
  ///     // ... extract did not consume all CLI arguments ...
  ///   }
  ///   if (res.opts.count("verbose") > 0) {
  ///     // ... enable verbose mode ...
  ///   }
  ///   // ...
  /// }
  /// ~~~
  /// @param xs List of argument descriptors.
  /// @param help_generator Optional factory function to generate help text
  ///                       (overrides the default generator).
  /// @param suppress_help Suppress generation of default-generated help option.
  /// @returns A struct containing remainder
  ///          (i.e. unmatched elements), a set containing the names of all
  ///          used arguments, and the generated help text.
  /// @throws std::invalid_argument if no name or more than one long name is set
  cli_res extract_opts(std::vector<cli_arg> xs,
                       help_factory help_generator = nullptr,
                       bool suppress_help = false) const;

  // -- inline observers -------------------------------------------------------

  /// Returns a const pointer to the element at position `p`.
  inline const void* at(size_t p) const noexcept {
    CAF_ASSERT(vals_);
    return vals_->get(p);
  }

  /// Returns a reference to the content.
  inline const data_ptr& vals() const noexcept {
    return vals_;
  }

  /// Returns a reference to the content.
  inline const data_ptr& cvals() const noexcept {
    return vals_;
  }

  /// Returns a type hint for the pattern matching engine.
  inline uint32_t type_token() const noexcept {
    return vals_ ? vals_->type_token() : 0xFFFFFFFF;
  }

  /// Returns whether there are more than one references to the content.
  inline bool shared() const noexcept {
    return vals_ ? vals_->shared() : false;
  }

  /// Returns the size of this message.
  inline size_t size() const noexcept {
    return vals_ ? vals_->size() : 0;
  }

  /// Creates a new message from the first n values.
  inline message take(size_t n) const {
    return n >= size() ? *this : drop_right(size() - n);
  }

  /// Creates a new message from the last n values.
  inline message take_right(size_t n) const {
    return n >= size() ? *this : drop(size() - n);
  }

  /// Returns true if `size() == 0, otherwise false.
  inline bool empty() const {
    return size() == 0;
  }

  /// Returns the value at position `p` as const reference of type `T`.
  template <class T>
  const T& get_as(size_t p) const {
    CAF_ASSERT(match_element(p, type_nr<T>::value, &typeid(T)));
    return *reinterpret_cast<const T*>(at(p));
  }

  /// Queries whether the element at position `p` is of type `T`.
  template <class T>
  bool match_element(size_t p) const noexcept {
    auto rtti = type_nr<T>::value == 0 ? &typeid(T) : nullptr;
    return match_element(p, type_nr<T>::value, rtti);
  }

  /// Queries whether the types of this message are `Ts...`.
  template <class... Ts>
  bool match_elements() const noexcept {
    std::integral_constant<size_t, 0> p0;
    detail::type_list<Ts...> tlist;
    return size() == sizeof...(Ts) && match_elements_impl(p0, tlist);
  }

  /// Queries the run-time type information for the element at position `pos`.
  inline std::pair<uint16_t, const std::type_info*>
  type(size_t pos) const noexcept {
    CAF_ASSERT(vals_ && vals_->size() > pos);
    return vals_->type(pos);
  }

  /// Checks whether the type of the stored value at position `pos`
  /// matches type number `n` and run-time type information `p`.
  bool match_element(size_t pos, uint16_t n,
                     const std::type_info* p) const noexcept {
    CAF_ASSERT(vals_);
    return vals_->matches(pos, n, p);
  }

  template <class T, class... Ts>
  bool match_elements(detail::type_list<T, Ts...> list) const noexcept {
    std::integral_constant<size_t, 0> p0;
    return size() == (sizeof...(Ts) + 1) && match_elements_impl(p0, list);
  }

private:
  template <size_t P>
  static bool match_elements_impl(std::integral_constant<size_t, P>,
                                  detail::type_list<>) noexcept {
    return true; // end of recursion
  }

  template <size_t P, class T, class... Ts>
  bool match_elements_impl(std::integral_constant<size_t, P>,
                           detail::type_list<T, Ts...>) const noexcept {
    std::integral_constant<size_t, P + 1> next_p;
    detail::type_list<Ts...> next_list;
    return match_element<T>(P) && match_elements_impl(next_p, next_list);
  }

  message extract_impl(size_t start, message_handler handler) const;

  static message concat_impl(std::initializer_list<data_ptr> ptrs);

  data_ptr vals_;
};

/// @relates message
void serialize(serializer& sink, const message& msg, const unsigned int);

/// @relates message
void serialize(deserializer& sink, message& msg, const unsigned int);

/// @relates message
std::string to_string(const message& msg);

/// Stores the name of a command line option ("<long name>[,<short name>]")
/// along with a description and a callback.
struct message::cli_arg {
  /// Returns `true` on a match, `false` otherwise.
  using consumer = std::function<bool (const std::string&)>;

  /// Full name of this CLI argument using format "<long name>[,<short name>]"
  std::string name;

  /// Desciption of this CLI argument for the auto-generated help text.
  std::string text;

  /// Auto-generated helptext for this item.
  std::string helptext;

  /// Evaluates option arguments.
  consumer fun;

  /// Set to true for zero-argument options.
  bool* flag;

  /// Creates a CLI argument without data.
  cli_arg(std::string name, std::string text);

  /// Creates a CLI flag option. The `flag` is set to `true` if the option
  /// was set, otherwise it is `false`.
  cli_arg(std::string name, std::string text, bool& flag);

  /// Creates a CLI argument storing its matched argument in `dest`.
  cli_arg(std::string name, std::string text, atom_value& dest);

  /// Creates a CLI argument storing its matched argument in `dest`.
  cli_arg(std::string name, std::string text, std::string& dest);

  /// Creates a CLI argument appending matched arguments to `dest`.
  cli_arg(std::string name, std::string text, std::vector<std::string>& dest);

  /// Creates a CLI argument using the function object `f`.
  cli_arg(std::string name, std::string text, consumer f);

  /// Creates a CLI argument for converting from strings,
  /// storing its matched argument in `dest`.
  template <class T>
  cli_arg(typename std::enable_if<
            type_nr<T>::value != 0,
            std::string
          >::type nstr,
          std::string tstr, T& arg)
      : name(std::move(nstr)),
        text(std::move(tstr)),
        flag(nullptr) {
    fun = [&arg](const std::string& str) -> bool {
      T x;
      // TODO: using this stream is a workaround for the missing
      //       from_string<T>() interface and has downsides such as
      //       not performing overflow/underflow checks etc.
      std::istringstream iss{str};
      if (iss >> x) {
        arg = x;
        return true;
      }
      return false;
    };
  }

  /// Creates a CLI argument for converting from strings,
  /// appending matched arguments to `dest`.
  template <class T>
  cli_arg(std::string nstr, std::string tstr, std::vector<T>& arg)
      : name(std::move(nstr)),
        text(std::move(tstr)),
        flag(nullptr) {
    fun = [&arg](const std::string& str) -> bool {
      T x;
      std::istringstream iss{ str };
      if (iss >> x) {
        arg.emplace_back(std::move(x));
        return true;
      }
      return false;
    };
  }
};

/// Stores the result of `message::extract_opts`.
struct message::cli_res {
  /// Stores the remaining (unmatched) arguments.
  message remainder;
  /// Stores the names of all active options.
  std::set<std::string> opts;
  /// Stores the automatically generated help text.
  std::string helptext;
  /// Stores errors during option parsing.
  std::string error;
};

/// @relates message
inline message operator+(const message& lhs, const message& rhs) {
  return message::concat(lhs, rhs);
}

} // namespace caf

#endif // CAF_MESSAGE_HPP
