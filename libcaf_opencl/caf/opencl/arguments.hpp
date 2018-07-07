/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Raphael Hiesgen <raphael.hiesgen (at) haw-hamburg.de>                      *
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

#include <functional>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/optional.hpp"

#include "caf/opencl/mem_ref.hpp"

namespace caf {
namespace detail {

template <class T, class F>
std::function<optional<T> (message&)> res_or_none(F fun) {
 return [fun](message& msg) -> optional<T> {
    auto res = msg.apply(fun);
    T result;
    if (res) {
      res->apply([&](size_t x) { result = x; });
      return result;
    }
    return none;
  };
}

template <class F, class T>
T try_apply_fun(F& fun, message& msg, const T& fallback) {
  if (fun) {
    auto res = fun(msg);
    if (res)
      return *res;
  }
  return fallback;
}

} // namespace detail

namespace opencl {

// Tag classes to mark arguments received in a messages as reference or value
/// Arguments tagged as `val` are expected as a vector (or value in case
/// of a private argument).
struct val {};

/// Arguments tagged as `mref` are expected as mem_ref, which is can be returned
/// by other opencl actors.
struct mref {};

/// Arguments tagged as `hidden` are created by the actor, using the config
/// passed in the argument wrapper. Only available for local and priv arguments.
struct hidden {};

/// Use as a default way to calculate output size. 0 will be set to the number
/// of work items at runtime.
struct dummy_size_calculator {
  template <class... Ts>
  size_t operator()(Ts&&...) const {
    return 0;
  }
};

/// Common parent for opencl argument tags for the spawn function.
struct arg_tag {};

/// Empty tag as an alternative for conditional inheritance.
struct empty_tag {};

/// Tags the argument as input which requires initialization through a message.
struct input_tag {};

/// Tags the argument as output which includes its buffer in the result message.
struct output_tag {};

/// Tags the argument to require specification of the size of its buffer.
struct requires_size_tag {};

/// Tags the argument as a reference and not a value.
struct is_ref_tag;

/// Mark a spawn argument as input only
template <class Arg, class Tag = val>
struct in : arg_tag, input_tag {
  static_assert(std::is_same<Tag, val>::value || std::is_same<Tag, mref>::value,
                "Argument of type `in` must be passed as value or mem_ref.");
  using tag_type = Tag;
  using arg_type = detail::decay_t<Arg>;
};

/// Mark a spawn argument as input and output
template <class Arg, class TagIn = val, class TagOut = val>
struct in_out : arg_tag, input_tag, output_tag {
  static_assert(
    std::is_same<TagIn, val>::value || std::is_same<TagIn, mref>::value,
    "Argument of type `in_out` must be passed as value or mem_ref."
  );
  static_assert(
    std::is_same<TagOut, val>::value || std::is_same<TagOut, mref>::value,
    "Argument of type `in_out` must be returned as value or mem_ref."
  );
  using tag_in_type = TagIn;
  using tag_out_type = TagOut;
  using arg_type = detail::decay_t<Arg>;
};

/// Mark a spawn argument as output only
template <class Arg, class Tag = val>
struct out : arg_tag, output_tag, requires_size_tag {
  static_assert(std::is_same<Tag, val>::value || std::is_same<Tag, mref>::value,
               "Argument of type `out` must be returned as value or mem_ref.");
  using tag_type = Tag;
  using arg_type = detail::decay_t<Arg>;
  out() = default;
  template <class F>
  out(F fun) : fun_{detail::res_or_none<size_t>(fun)} {
    // nop
  }
  optional<size_t> operator()(message& msg) const {
    return detail::try_apply_fun(fun_, msg, 0UL);
  }
  std::function<optional<size_t> (message&)> fun_;
};

/// Mark a spawn argument as on-device scratch space
template <class Arg>
struct scratch : arg_tag, requires_size_tag {
  using arg_type = detail::decay_t<Arg>;
  scratch() = default;
  template <class F>
  scratch(F fun) : fun_{detail::res_or_none<size_t>(fun)} {
    // nop
  }
  optional<size_t> operator()(message& msg) const {
    return detail::try_apply_fun(fun_, msg, 0UL);
  }
  std::function<optional<size_t> (message&)> fun_;
};

/// Mark a spawn argument as a local memory argument. This argument cannot be
/// initalized from the CPU, but requires specification of its size. An
/// optional function allows calculation of the size depeding on the input
/// message.
template <class Arg>
struct local : arg_tag, requires_size_tag {
  using arg_type = detail::decay_t<Arg>;
  local() = default;
  local(size_t size) : size_(size) { }
  template <class F>
  local(size_t size, F fun)
    : size_(size), fun_{detail::res_or_none<size_t>(fun)} {
    // nop
  }
  size_t operator()(message& msg) const {
    return detail::try_apply_fun(fun_, msg, size_);
  }
  size_t size_;
  std::function<optional<size_t> (message&)> fun_;
};

/// Mark a spawn argument as a private argument. Requires a default value but
/// can optionally be calculated depending on the input through a passed
/// function.
template <class Arg, class Tag = hidden>
struct priv : arg_tag, std::conditional<std::is_same<Tag, val>::value,
                                        input_tag, empty_tag>::type {
  static_assert(std::is_same<Tag, val>::value ||
                std::is_same<Tag, hidden>::value,
               "Argument of type `priv` must be either a value or hidden.");
  using tag_type = Tag;
  using arg_type = detail::decay_t<Arg>;
  priv() = default;
  priv(Arg val) : value_(val) {
    static_assert(std::is_same<Tag, hidden>::value,
                  "Argument of type `priv` can only be initialized with a value"
                  " if it is tagged as hidden.");
  }
  template <class F>
  priv(Arg val, F fun) : value_(val), fun_{detail::res_or_none<Arg>(fun)} {
    static_assert(std::is_same<Tag, hidden>::value,
                  "Argument of type `priv` can only be initialized with a value"
                  " if it is tagged as hidden.");
  }
  Arg operator()(message& msg) const {
    return detail::try_apply_fun(fun_, msg, value_);
  }
  Arg value_;
  std::function<optional<Arg> (message&)> fun_;
};

///Cconverts C arrays, i.e., pointers, to vectors.
template <class T>
struct carr_to_vec {
  using type = T;
};

template <class T>
struct carr_to_vec<T*> {
  using type = std::vector<T>;
};

/// Filter types for any argument type.
template <class T>
struct is_opencl_arg : std::is_base_of<arg_tag, T> {};

/// Filter type lists for input arguments
template <class T>
struct is_input_arg : std::is_base_of<input_tag, T> {};

/// Filter type lists for output arguments
template <class T>
struct is_output_arg : std::is_base_of<output_tag, T> {};

/// Filter for arguments that require size
template <class T>
struct requires_size_arg : std::is_base_of<requires_size_tag, T> {};

/// Filter mem_refs
template <class T>
struct is_ref_type : std::is_base_of<is_ref_tag, T> {};

template <class T>
struct is_val_type
  : std::integral_constant<bool, !std::is_base_of<is_ref_tag, T>::value> {};

/// extract types
template <class T>
struct extract_type { };

template <class T, class Tag>
struct extract_type<in<T, Tag>> {
  using type = detail::decay_t<typename carr_to_vec<T>::type>;
};

template <class T, class TagIn, class TagOut>
struct extract_type<in_out<T, TagIn, TagOut>> {
  using type = detail::decay_t<typename carr_to_vec<T>::type>;
};

template <class T, class Tag>
struct extract_type<out<T, Tag>> {
  using type = detail::decay_t<typename carr_to_vec<T>::type>;
};

template <class T>
struct extract_type<scratch<T>> {
  using type = detail::decay_t<typename carr_to_vec<T>::type>;
};

template <class T>
struct extract_type<local<T>> {
  using type = detail::decay_t<typename carr_to_vec<T>::type>;
};

template <class T, class Tag>
struct extract_type<priv<T, Tag>> {
  using type = detail::decay_t<typename carr_to_vec<T>::type>;
};

/// extract type expected in an incoming message
template <class T>
struct extract_input_type { };

template <class Arg>
struct extract_input_type<in<Arg, val>> {
  using type = std::vector<Arg>;
};

template <class Arg>
struct extract_input_type<in<Arg, mref>> {
  using type = opencl::mem_ref<Arg>;
};

template <class Arg, class TagOut>
struct extract_input_type<in_out<Arg, val, TagOut>> {
  using type = std::vector<Arg>;
};

template <class Arg, class TagOut>
struct extract_input_type<in_out<Arg, mref, TagOut>> {
  using type = opencl::mem_ref<Arg>;
};

template <class Arg>
struct extract_input_type<priv<Arg,val>> {
  using type = Arg;
};

/// extract type sent in an outgoing message
template <class T>
struct extract_output_type { };

template <class Arg>
struct extract_output_type<out<Arg, val>> {
  using type = std::vector<Arg>;
};

template <class Arg>
struct extract_output_type<out<Arg, mref>> {
  using type = opencl::mem_ref<Arg>;
};

template <class Arg, class TagIn>
struct extract_output_type<in_out<Arg, TagIn, val>> {
  using type = std::vector<Arg>;
};

template <class Arg, class TagIn>
struct extract_output_type<in_out<Arg, TagIn, mref>> {
  using type = opencl::mem_ref<Arg>;
};

/// extract input tag
template <class T>
struct extract_input_tag { };

template <class Arg, class Tag>
struct extract_input_tag<in<Arg, Tag>> {
  using tag = Tag;
};

template <class Arg, class TagIn, class TagOut>
struct extract_input_tag<in_out<Arg, TagIn, TagOut>> {
  using tag = TagIn;
};

template <class Arg>
struct extract_input_tag<priv<Arg,val>> {
  using tag = val;
};

/// extract output tag
template <class T>
struct extract_output_tag { };

template <class Arg, class Tag>
struct extract_output_tag<out<Arg, Tag>> {
  using tag = Tag;
};

template <class Arg, class TagIn, class TagOut>
struct extract_output_tag<in_out<Arg, TagIn, TagOut>> {
  using tag = TagOut;
};


/// Create the return message from tuple arumgent
struct message_from_results {
  template <class T, class... Ts>
  message operator()(T& x, Ts&... xs) {
    return make_message(std::move(x), std::move(xs)...);
  }
  template <class... Ts>
  message operator()(std::tuple<Ts...>& values) {
    return apply_args(*this, detail::get_indices(values), values);
  }
};

/// Calculate output indices from the kernel message

// index in output tuple
template <int Counter, class Arg>
struct out_index_of {
  static constexpr int value = -1;
  static constexpr int next = Counter;
};

template <int Counter, class Arg, class TagIn, class TagOut>
struct out_index_of<Counter, in_out<Arg,TagIn,TagOut>> {
  static constexpr int value = Counter;
  static constexpr int next = Counter + 1;
};

template <int Counter, class Arg, class Tag>
struct out_index_of<Counter, out<Arg,Tag>> {
  static constexpr int value = Counter;
  static constexpr int next = Counter + 1;
};

// index in input message
template <int Counter, class Arg>
struct in_index_of {
  static constexpr int value = -1;
  static constexpr int next = Counter;
};

template <int Counter, class Arg, class Tag>
struct in_index_of<Counter, in<Arg,Tag>> {
  static constexpr int value = Counter;
  static constexpr int next = Counter + 1;
};

template <int Counter, class Arg, class TagIn, class TagOut>
struct in_index_of<Counter, in_out<Arg,TagIn,TagOut>> {
  static constexpr int value = Counter;
  static constexpr int next = Counter + 1;
};

template <int Counter, class Arg>
struct in_index_of<Counter, priv<Arg,val>> {
  static constexpr int value = Counter;
  static constexpr int next = Counter + 1;
};


template <int In, int Out, class T>
struct cl_arg_info {
  static constexpr int in_pos = In;
  static constexpr int out_pos = Out;
  using type = T;
};

template <class ListA, class ListB, int InCounter, int OutCounter>
struct cl_arg_info_list_impl;

template <class Arg, class... Remaining, int InCounter, int OutCounter>
struct cl_arg_info_list_impl<detail::type_list<>,
                             detail::type_list<Arg, Remaining...>,
                             InCounter, OutCounter> {
  using in_idx = in_index_of<InCounter, Arg>;
  using out_idx = out_index_of<OutCounter, Arg>;
  using type =
    typename cl_arg_info_list_impl<
      detail::type_list<cl_arg_info<in_idx::value, out_idx::value, Arg>>,
      detail::type_list<Remaining...>,
      in_idx::next, out_idx::next
    >::type;
};

template <class... Args, class Arg, class... Remaining,
          int InCounter, int OutCounter>
struct cl_arg_info_list_impl<detail::type_list<Args...>,
                             detail::type_list<Arg, Remaining...>,
                             InCounter, OutCounter> {
  using in_idx = in_index_of<InCounter, Arg>;
  using out_idx = out_index_of<OutCounter, Arg>;
  using type =
    typename cl_arg_info_list_impl<
      detail::type_list<Args..., cl_arg_info<in_idx::value, out_idx::value, Arg>>,
      detail::type_list<Remaining...>,
      in_idx::next, out_idx::next
    >::type;
};

template <class... Args, int InCounter, int OutCounter>
struct cl_arg_info_list_impl<detail::type_list<Args...>,
                             detail::type_list<>,
                             InCounter, OutCounter> {
  using type = detail::type_list<Args...>;
};

template <class List>
struct cl_arg_info_list {
  using type = typename cl_arg_info_list_impl<
    detail::type_list<>,
    List, 0, 0
  >::type;
};

/// Helpers for conversion in deprecated spawn functions

template <class T>
struct to_input_arg {
  using type = in<T>;
};

template <class T>
struct to_output_arg {
  using type = out<T>;
};

} // namespace opencl
} // namespace caf

