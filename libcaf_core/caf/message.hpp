/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <tuple>
#include <sstream>
#include <type_traits>

#include "caf/atom.hpp"
#include "caf/config.hpp"
#include "caf/fwd.hpp"
#include "caf/index_mapping.hpp"
#include "caf/make_counted.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/skip.hpp"
#include "caf/type_nr.hpp"

#include "caf/detail/apply_args.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/implicit_conversions.hpp"
#include "caf/detail/int_list.hpp"
#include "caf/detail/message_data.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {
class message_handler;

/// Describes a fixed-length, copy-on-write, type-erased
/// tuple with elements of any type.
class message : public type_erased_tuple {
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
  message(none_t) noexcept;
  message(const message&) noexcept = default;
  message& operator=(const message&) noexcept = default;

  message(message&&) noexcept;
  message& operator=(message&&) noexcept;
  explicit message(data_ptr  ptr) noexcept;

  ~message() override;

  // -- implementation of type_erased_tuple ------------------------------------

  void* get_mutable(size_t p) override;

  error load(size_t pos, deserializer& source) override;

  size_t size() const noexcept override;

  uint32_t type_token() const noexcept override;

  rtti_pair type(size_t pos) const noexcept override;

  const void* get(size_t pos) const noexcept override;

  std::string stringify(size_t pos) const override;

  type_erased_value_ptr copy(size_t pos) const override;

  error save(size_t pos, serializer& sink) const override;

  bool shared() const noexcept override;

  error load(deserializer& source) override;

  error save(serializer& sink) const override;

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
  message slice(size_t pos, size_t n) const;

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
  /// @param f Optional factory function to generate help text
  ///          (overrides the default generator).
  /// @param no_help Suppress generation of default-generated help option.
  /// @returns A struct containing remainder
  ///          (i.e. unmatched elements), a set containing the names of all
  ///          used arguments, and the generated help text.
  /// @throws std::invalid_argument if no name or more than one long name is set
  cli_res extract_opts(std::vector<cli_arg> xs,
                       const help_factory& f = nullptr,
                       bool no_help = false) const;

  // -- inline observers -------------------------------------------------------

  /// Returns a const pointer to the element at position `p`.
  inline const void* at(size_t p) const noexcept {
    CAF_ASSERT(vals_ != nullptr);
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

  /// Returns the size of this message.
  /// Creates a new message from the first n values.
  inline message take(size_t n) const {
    return n >= size() ? *this : drop_right(size() - n);
  }

  /// Creates a new message from the last n values.
  inline message take_right(size_t n) const {
    return n >= size() ? *this : drop(size() - n);
  }

  /// @cond PRIVATE

  /// @pre `!empty()`
  inline type_erased_tuple& content() {
    CAF_ASSERT(vals_ != nullptr);
    return *vals_;
  }

  inline const type_erased_tuple& content() const {
    CAF_ASSERT(vals_ != nullptr);
    return *vals_;
  }

  /// @endcond

private:
  // -- private helpers --------------------------------------------------------

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

  static message concat_impl(std::initializer_list<data_ptr> xs);

  // -- member functions -------------------------------------------------------

  data_ptr vals_;
};

// -- nested types -------------------------------------------------------------

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
  cli_arg(std::string nstr, std::string tstr);

  /// Creates a CLI flag option. The `flag` is set to `true` if the option
  /// was set, otherwise it is `false`.
  cli_arg(std::string nstr, std::string tstr, bool& arg);

  /// Creates a CLI argument storing its matched argument in `dest`.
  cli_arg(std::string nstr, std::string tstr, atom_value& arg);

  /// Creates a CLI argument storing its matched argument in `dest`.
  cli_arg(std::string nstr, std::string tstr, timespan& arg);

  /// Creates a CLI argument storing its matched argument in `dest`.
  cli_arg(std::string nstr, std::string tstr, std::string& arg);

  /// Creates a CLI argument appending matched arguments to `dest`.
  cli_arg(std::string nstr, std::string tstr, std::vector<std::string>& arg);

  /// Creates a CLI argument using the function object `f`.
  cli_arg(std::string nstr, std::string tstr, consumer f);

  /// Creates a CLI argument for converting from strings,
  /// storing its matched argument in `dest`.
  template <class T>
  cli_arg(std::string nstr, std::string tstr, T& arg)
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
      std::istringstream iss{str};
      if (iss >> x) {
        arg.emplace_back(std::move(x));
        return true;
      }
      return false;
    };
  }
};

// -- related non-members ------------------------------------------------------

/// @relates message
error inspect(serializer& sink, message& msg);

/// @relates message
error inspect(deserializer& source, message& msg);

/// @relates message
std::string to_string(const message& msg);

/// @relates message
inline message operator+(const message& lhs, const message& rhs) {
  return message::concat(lhs, rhs);
}

} // namespace caf

