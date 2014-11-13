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

#ifndef CAF_REPLIES_TO_HPP
#define CAF_REPLIES_TO_HPP

#include "caf/uniform_type_info.hpp"
#include "caf/detail/type_list.hpp"

namespace caf {

template <class... Is>
struct replies_to {
  template <class... Os>
  struct with {
    static_assert(sizeof...(Is) > 0, "template parameter pack Is empty");
    static_assert(sizeof...(Os) > 0, "template parameter pack Os empty");
    using input_types = detail::type_list<Is...>;
    using output_types = detail::type_list<Os...>;
    static std::string as_string() {
      const uniform_type_info* inputs[] = {uniform_typeid<Is>(true)...};
      const uniform_type_info* outputs[] = {uniform_typeid<Os>(true)...};
      std::string result;
      // 'void' is not an announced type, hence we check whether uniform_typeid
      // did return a valid pointer to identify 'void' (this has the
      // possibility of false positives, but those will be catched anyways)
      auto plot = [&](const uniform_type_info** arr) {
        auto uti = arr[0];
        result += uti ? uti->name() : "void";
        for (size_t i = 1; i < sizeof...(Is); ++i) {
          uti = arr[i];
          result += ",";
          result += uti ? uti->name() : "void";
        }
      };
      result = "caf::replies_to<";
      plot(inputs);
      result += ">::with<";
      plot(outputs);
      result += ">";
      return result;
    }
  };
};

template <class... Is>
using reacts_to = typename replies_to<Is...>::template with<void>;

} // namespace caf

#endif // CAF_REPLIES_TO_HPP
