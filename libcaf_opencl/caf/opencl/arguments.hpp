/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
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

#ifndef CAF_OPENCL_ARGUMENTS
#define CAF_OPENCL_ARGUMENTS

#include <functional>
#include <type_traits>

#include "caf/message.hpp"
#include "caf/optional.hpp"

#include "caf/opencl/mem_ref.hpp"

namespace caf {
namespace opencl {

// Tag classes to mark arguments received in a messages as reference or value
/// Arguments tagged as `val` are expected as a vector (or value in case
/// of a private argument). 
struct val { };

/// Arguments tagged as `mref` are expected as mem_ref, which is can be returned
/// by other opencl actors.
struct mref { };

/// Arguments tagged as `hidden` are created by the actor, using the config
/// passed in the argument wrapper. Only available for local and priv arguments.
struct hidden { };

/// Use as a default way to calculate output size. 0 will be set to the number
/// of work items at runtime.
struct dummy_size_calculator {
  template <class... Ts>
  size_t operator()(Ts&&...) const {
    return 0;
  }
};

/// Mark an a spawn_cl template argument as input only
template <class Arg, class Tag = val>
struct in {
  static_assert(std::is_same<Tag,val>::value || std::is_same<Tag,mref>::value,
                "Argument of type `in` must be passed as value or mem_ref.");
  using tag_type = Tag;
  using arg_type = typename std::decay<Arg>::type;
};

/// Mark an a spawn_cl template argument as input and output
template <class Arg, class TagIn = val, class TagOut = val>
struct in_out {
  static_assert(
    std::is_same<TagIn,val>::value || std::is_same<TagIn,mref>::value,
    "Argument of type `in_out` must be passed as value or mem_ref."
  );
  static_assert(
    std::is_same<TagOut,val>::value || std::is_same<TagOut,mref>::value,
    "Argument of type `in_out` must be returned as value or mem_ref."
  );
  using tag_in_type = TagIn;
  using tag_out_type = TagOut;
  using arg_type = typename std::decay<Arg>::type;
};

template <class Arg, class Tag = val>
struct out {
  static_assert(std::is_same<Tag,val>::value || std::is_same<Tag,mref>::value,
               "Argument of type `out` must be returned as value or mem_ref.");
  using tag_type = Tag;
  using arg_type = typename std::decay<Arg>::type;
  out() = default;
  template <class F>
  out(F fun) {
    fun_ = [fun](message& msg) -> optional<size_t> {
      auto res = msg.apply(fun);
      size_t result;
      if (res) {
        res->apply([&](size_t x) { result = x; });
        return result;
      }
      return none;
    };
  }
  optional<size_t> operator()(message& msg) const {
    return fun_ ? fun_(msg) : 0UL;
  }
  std::function<optional<size_t> (message&)> fun_;
};

template <class Arg>
struct scratch {
  using arg_type = typename std::decay<Arg>::type;
  scratch() = default;
  template <class F>
  scratch(F fun) {
    fun_ = [fun](message& msg) -> optional<size_t> {
      auto res = msg.apply(fun);
      size_t result;
      if (res) {
        res->apply([&](size_t x) { result = x; });
        return result;
      }
      return none;
    };
  }
  optional<size_t> operator()(message& msg) const {
    return fun_ ? fun_(msg) : 0UL;
  }
  std::function<optional<size_t> (message&)> fun_;
};

/// Argument placed in local memory. Cannot be initalized from the CPU, but
/// requires a size that is calculated depending on the input.
template <class Arg>
struct local {
  using arg_type = typename std::decay<Arg>::type;
  local() = default;
  template <class F>
  local(size_t size, F fun) : size_(size) {
    fun_ = [fun](message& msg) -> optional<size_t> {
      auto res = msg.apply(fun);
      size_t result;
      if (res) {
        res->apply([&](size_t x) { result = x; });
        return result;
      }
      return none;
    };
  }
  local(size_t size) : size_(size) { }
  size_t operator()(message& msg) const {
    if (fun_) {
      auto res = fun_(msg);
      if (res)
        return *res;
    }
    return size_;
  }
  size_t size_;
  std::function<optional<size_t> (message&)> fun_;
};

/// Argument placed in private memory. Requires a default value but can
/// alternatively be calculated depending on the input through a function
/// passed to the constructor.
template <class Arg, class Tag = hidden>
struct priv {
  static_assert(std::is_same<Tag,val>::value || std::is_same<Tag,hidden>::value,
               "Argument of type `priv` must be returned as value or hidden.");
  using tag_type = Tag;
  using arg_type = typename std::decay<Arg>::type;
  priv() = default;
  template <class F>
  priv(Arg val, F fun) : value_(val) {
    static_assert(std::is_same<Tag,hidden>::value,
               "Argument of type `priv` can only be initialized with a value"
               "if it is manged by the actor, i.e., tagged as hidden.");
    fun_ = [fun](message& msg) -> optional<Arg> {
      auto res = msg.apply(fun);
      Arg result;
      if (res) {
        res->apply([&](Arg x) { result = x; });
        return result;
      }
      return none;
    };
  }
  priv(Arg val) : value_(val) {
    static_assert(std::is_same<Tag,hidden>::value,
               "Argument of type `priv` can only be initialized with a value"
               "if it is manged by the actor, i.e., tagged as hidden.");
  }
  Arg operator()(message& msg) const {
    if (fun_) {
      auto res = fun_(msg);
      if (res)
        return *res;
    }
    return value_;
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
struct is_opencl_arg : std::false_type {};

template <class T, class Tag>
struct is_opencl_arg<in<T, Tag>> : std::true_type {};

template <class T, class TagIn, class TagOut>
struct is_opencl_arg<in_out<T, TagIn, TagOut>> : std::true_type {};

template <class T, class Tag>
struct is_opencl_arg<out<T, Tag>> : std::true_type {};

template <class T>
struct is_opencl_arg<scratch<T>> : std::true_type {};

template <class T>
struct is_opencl_arg<local<T>> : std::true_type {};

template <class T, class Tag>
struct is_opencl_arg<priv<T, Tag>> : std::true_type {};

/// Filter type lists for input arguments
template <class T>
struct is_input_arg : std::false_type {};

template <class T, class Tag>
struct is_input_arg<in<T, Tag>> : std::true_type {};

template <class T, class TagIn, class TagOut>
struct is_input_arg<in_out<T, TagIn, TagOut>> : std::true_type {};

template <class T>
struct is_input_arg<priv<T,val>> : std::true_type {};

/// Filter type lists for output arguments
template <class T>
struct is_output_arg : std::false_type {};

template <class T, class Tag>
struct is_output_arg<out<T, Tag>> : std::true_type {};

template <class T, class TagIn, class TagOut>
struct is_output_arg<in_out<T, TagIn, TagOut>> : std::true_type {};

/// Filter for arguments that require size
template <class T>
struct requires_size_arg : std::false_type {};

template <class T, class Tag>
struct requires_size_arg<out<T, Tag>> : std::true_type {};

template <class T>
struct requires_size_arg<scratch<T>> : std::true_type {};

template <class T>
struct requires_size_arg<local<T>> : std::true_type {};

//template <class T, class Tag>
//struct requires_size_arg<priv<T, Tag>> : std::true_type {};

/// Filter mem_refs
template <class T>
struct is_ref_type : std::false_type {};

template <class T>
struct is_ref_type<mem_ref<T>> : std::true_type {};

template <class T>
struct is_val_type : std::true_type {};

template <class T>
struct is_val_type<mem_ref<T>> : std::false_type {};

/// extract types
template <class T>
struct extract_type { };

template <class T, class Tag>
struct extract_type<in<T, Tag>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
};

template <class T, class TagIn, class TagOut>
struct extract_type<in_out<T, TagIn, TagOut>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
};

template <class T, class Tag>
struct extract_type<out<T, Tag>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
};

template <class T>
struct extract_type<scratch<T>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
};

template <class T>
struct extract_type<local<T>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
};

template <class T, class Tag>
struct extract_type<priv<T, Tag>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
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
struct cl_arg_info_list_impl<detail::type_list<Args...>, detail::type_list<>,
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

#endif // CAF_OPENCL_ARGUMENTS
