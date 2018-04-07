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
#include <cstddef>
#include <cstdint>
#include <typeinfo>
#include <functional>

#include "caf/fwd.hpp"
#include "caf/error.hpp"
#include "caf/type_nr.hpp"
#include "caf/type_erased_value.hpp"
#include "caf/type_erased_tuple.hpp"

#include "caf/detail/try_match.hpp"
#include "caf/detail/apply_args.hpp"

namespace caf {
namespace detail {

template <class... Ts>
class type_erased_tuple_view : public type_erased_tuple {
public:
  // -- member types -----------------------------------------------------------

  template <size_t X>
  using num_token = std::integral_constant<size_t, X>;

  using tuple_type = std::tuple<type_erased_value_impl<std::reference_wrapper<Ts>>...>;

  // -- constructors, destructors, and assignment operators --------------------

  type_erased_tuple_view(Ts&... xs) : xs_(xs...) {
    init();
  }

  type_erased_tuple_view(const type_erased_tuple_view& other)
      : type_erased_tuple(),
        xs_(other.xs_) {
    init();
  }

  type_erased_tuple_view& operator=(type_erased_tuple_view&&) = delete;
  type_erased_tuple_view& operator=(const type_erased_tuple_view&) = delete;

  // -- overridden modifiers ---------------------------------------------------

  void* get_mutable(size_t pos) override {
    return ptrs_[pos]->get_mutable();
  }

  error load(size_t pos, deserializer& source) override {
    return ptrs_[pos]->load(source);
  }

  // -- overridden observers ---------------------------------------------------

  size_t size() const noexcept override {
    return sizeof...(Ts);
  }

  uint32_t type_token() const noexcept override {
    return make_type_token<Ts...>();
  }

  rtti_pair type(size_t pos) const noexcept override {
    return ptrs_[pos]->type();
  }

  const void* get(size_t pos) const noexcept override {
    return ptrs_[pos]->get();
  }

  std::string stringify(size_t pos) const override {
    return ptrs_[pos]->stringify();
  }

  type_erased_value_ptr copy(size_t pos) const override {
    return ptrs_[pos]->copy();
  }

  error save(size_t pos, serializer& sink) const override {
    return ptrs_[pos]->save(sink);
  }

  // -- member variables access ------------------------------------------------

  tuple_type& data() {
    return xs_;
  }

  const tuple_type& data() const {
    return xs_;
  }

private:
  // -- pointer "lookup table" utility -----------------------------------------

  template <size_t N>
  void init(num_token<N>, num_token<N>) {
    // end of recursion
  }

  template <size_t P, size_t N>
  void init(num_token<P>, num_token<N> last) {
    ptrs_[P] = &std::get<P>(xs_);
    init(num_token<P + 1>{}, last);
  }

  void init() {
    init(num_token<0>{}, num_token<sizeof...(Ts)>{});
  }

  // -- data members -----------------------------------------------------------

  tuple_type xs_;
  type_erased_value* ptrs_[sizeof...(Ts) == 0 ? 1 : sizeof...(Ts)];
};

} // namespace detail
} // namespace caf

