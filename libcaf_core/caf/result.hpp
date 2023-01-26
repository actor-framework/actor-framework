// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/delegated.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/message.hpp"
#include "caf/none.hpp"
#include "caf/skip.hpp"
#include "caf/variant_wrapper.hpp"

#include <type_traits>
#include <variant>

namespace caf::detail {

// Tag type for selecting a protected constructor in `result_base`.
struct result_base_message_init {};

} // namespace caf::detail

namespace caf {

// -- base type for result<Ts...> ----------------------------------------------

/// Base type for all specializations of `result`.
template <class... Ts>
class result_base {
public:
  static_assert(sizeof...(Ts) > 0);

  using types = detail::type_list<delegated<Ts...>, message, error>;

  result_base() = default;

  result_base(result_base&&) = default;

  result_base(const result_base&) = default;

  result_base& operator=(result_base&&) = default;

  result_base& operator=(const result_base&) = default;

  template <class Enum, class = std::enable_if_t<is_error_code_enum_v<Enum>>>
  result_base(Enum x) : content_(make_error(x)) {
    // nop
  }

  result_base(error x) : content_(std::move(x)) {
    // nop
  }

  result_base(delegated<Ts...> x) : content_(x) {
    // nop
  }

  result_base(const typed_response_promise<Ts...>&)
    : content_(delegated<Ts...>{}) {
    // nop
  }

  result_base(const response_promise&) : content_(delegated<Ts...>{}) {
    // nop
  }

  /// @private
  auto& get_data() & {
    return content_;
  }

  /// @private
  const auto& get_data() const& {
    return content_;
  }

  /// @private
  auto&& get_data() && {
    return std::move(content_);
  }

protected:
  explicit result_base(detail::result_base_message_init) : content_(message{}) {
    // nop
  }

  template <class... Us>
  explicit result_base(detail::result_base_message_init, Us&&... xs)
    : content_(make_message(std::forward<Us>(xs)...)) {
    // nop
  }

  std::variant<delegated<Ts...>, message, error> content_;
};

// -- result<Ts...> and its specializations ------------------------------------

/// Wraps the result of a message handler to represent either a value (wrapped
/// into a `message`), a `delegated<Ts...>` (indicates that another actor is
/// going to respond), or an `error`.
template <class... Ts>
class result;

template <>
class result<void> : public result_base<void> {
public:
  using super = result_base<void>;

  using super::super;

  result() : super(detail::result_base_message_init{}) {
    // nop
  }

  result(unit_t) : super(detail::result_base_message_init{}) {
    // nop
  }

  result(delegated<unit_t>) : super(delegated<void>{}) {
    // nop
  }

  result(const typed_response_promise<unit_t>&) : super(delegated<void>{}) {
    // nop
  }
};

template <>
class result<unit_t> : public result_base<void> {
public:
  using super = result_base<void>;

  using super::super;

  result() : super(detail::result_base_message_init{}) {
    // nop
  }

  result(unit_t) : super(detail::result_base_message_init{}) {
    // nop
  }

  result(delegated<unit_t>) : super(delegated<void>{}) {
    // nop
  }

  result(const typed_response_promise<unit_t>&) : super(delegated<void>{}) {
    // nop
  }
};

template <>
class result<message> : public result_base<message> {
public:
  using super = result_base<message>;

  using super::super;

  result(message x) {
    this->content_ = std::move(x);
  }

  result(expected<message> x) {
    if (x)
      this->content_ = std::move(*x);
    else
      this->content_ = std::move(x.error());
  }

  result& operator=(expected<message> x) {
    if (x)
      this->content_ = std::move(*x);
    else
      this->content_ = std::move(x.error());
    return *this;
  }
};

template <class T>
class result<T> : public result_base<T> {
public:
  using super = result_base<T>;

  using super::super;

  template <class U,
            class = std::enable_if_t<std::is_constructible_v<T, U>
                                     && !std::is_constructible_v<super, U>>>
  result(U&& x)
    : super(detail::result_base_message_init{}, T{std::forward<U>(x)}) {
    // nop
  }

  result(expected<T> x) {
    if (x)
      this->content_ = make_message(std::move(*x));
    else
      this->content_ = std::move(x.error());
  }

  result& operator=(expected<T> x) {
    if (x)
      this->content_ = make_message(std::move(*x));
    else
      this->content_ = std::move(x.error());
    return *this;
  }
};

template <class T0, class T1, class... Ts>
class result<T0, T1, Ts...> : public result_base<T0, T1, Ts...> {
public:
  using super = result_base<T0, T1, Ts...>;

  using super::super;

  result(T0 x0, T1 x1, Ts... xs)
    : super(detail::result_base_message_init{}, std::move(x0), std::move(x1),
            std::move(xs)...) {
    // nop
  }
};

// -- free functions -----------------------------------------------------------

/// Convenience function for wrapping the parameter pack `xs...` into a
/// `result`.
template <class... Ts>
auto make_result(Ts&&... xs) {
  return result<std::decay_t<Ts>...>(std::forward<Ts>(xs)...);
}

// -- special type alias for a skippable result<message> -----------------------

/// Similar to `result<message>`, but also allows to *skip* a message.
class skippable_result {
public:
  skippable_result() = default;
  skippable_result(skippable_result&&) = default;
  skippable_result(const skippable_result&) = default;
  skippable_result& operator=(skippable_result&&) = default;
  skippable_result& operator=(const skippable_result&) = default;

  skippable_result(delegated<message> x) : content_(std::move(x)) {
    // nop
  }

  skippable_result(message x) : content_(std::move(x)) {
    // nop
  }

  skippable_result(error x) : content_(std::move(x)) {
    // nop
  }

  skippable_result(skip_t x) : content_(std::move(x)) {
    // nop
  }

  skippable_result(expected<message> x) {
    if (x)
      this->content_ = std::move(*x);
    else
      this->content_ = std::move(x.error());
  }

  skippable_result& operator=(expected<message> x) {
    if (x)
      this->content_ = std::move(*x);
    else
      this->content_ = std::move(x.error());
    return *this;
  }

  /// @private
  auto& get_data() {
    return content_;
  }

  /// @private
  const auto& get_data() const {
    return content_;
  }

private:
  std::variant<delegated<message>, message, error, skip_t> content_;
};

// -- type traits --------------------------------------------------------------

template <class T>
struct is_result : std::false_type {};

template <class... Ts>
struct is_result<result<Ts...>> : std::true_type {};

// -- enable std::variant-style interface --------------------------------------

template <class... Ts>
struct is_variant_wrapper<result<Ts...>> : std::true_type {};

template <>
struct is_variant_wrapper<skippable_result> : std::true_type {};

} // namespace caf
