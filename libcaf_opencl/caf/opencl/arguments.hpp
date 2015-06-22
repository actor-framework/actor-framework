/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

namespace caf {
namespace opencl {

/// Use as a default way to calculate output size. 0 will be set to the number
/// of work items at runtime.
struct dummy_size_calculator {
  template <class... Ts>
  size_t operator()(Ts&&...) const {
    return 0;
  }
};

/// Mark an a spawn_cl template argument as input only
template <class Arg>
struct in {
  using arg_type = typename std::decay<Arg>::type;
};

/// Mark an a spawn_cl template argument as input and output
template <class Arg>
struct in_out {
  using arg_type = typename std::decay<Arg>::type;
};

template <class Arg>
struct out {
  out() { }
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
    return fun_ ? fun_(msg) : 0;
  }
  std::function<optional<size_t> (message&)> fun_;
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

template <class T>
struct is_opencl_arg<in<T>> : std::true_type {};

template <class T>
struct is_opencl_arg<in_out<T>> : std::true_type {};

template <class T>
struct is_opencl_arg<out<T>> : std::true_type {};


/// Filter type lists for input arguments, in and in_out.
template <class T>
struct is_input_arg : std::false_type {};

template <class T>
struct is_input_arg<in<T>> : std::true_type {};

template <class T>
struct is_input_arg<in_out<T>> : std::true_type {};

/// Filter type lists for output arguments, in_out and out.
template <class T>
struct is_output_arg : std::false_type {};

template <class T>
struct is_output_arg<out<T>> : std::true_type {};

template <class T>
struct is_output_arg<in_out<T>> : std::true_type {};

/// Filter for arguments that require size, in this case only out.
template <class T>
struct requires_size_arg : std::false_type {};

template <class T>
struct requires_size_arg<out<T>> : std::true_type {};

/// extract types
template <class T>
struct extract_type { };

template <class T>
struct extract_type<in<T>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
};

template <class T>
struct extract_type<in_out<T>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
};

template <class T>
struct extract_type<out<T>> {
  using type = typename std::decay<typename carr_to_vec<T>::type>::type;
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
