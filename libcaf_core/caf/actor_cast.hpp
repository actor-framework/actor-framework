// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/abstract_actor.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/fwd.hpp"

#include <type_traits>

namespace caf {

namespace {

// The function actor_cast<> computes the type of the cast for
// actor_cast_access via the following formula:
//     x = 0 if To is a raw pointer
//       = 1 if To is a strong pointer
//       = 2 if To is a weak pointer
//     y = 0 if From is a raw pointer
//       = 6 if From is a strong pointer
//       = 2 if From is a weak pointer
// the result of x * y + z then denotes which operation the cast is performing:
//     raw              <- raw              =  0
//     raw              <- weak             =  0
//     raw              <- strong           =  0
//     weak             <- raw              =  0
//     weak             <- weak             =  6
//     weak             <- strong           = 12
//     strong           <- raw              =  0
//     strong           <- weak             =  3
//     strong           <- strong           =  6
// x * y is then interpreted as follows:
// -  0 is a conversion to or from a raw pointer
// -  6 is a conversion between pointers with same semantics
// -  3 is a conversion from a weak pointer to a strong pointer
// - 12 is a conversion from a strong pointer to a weak pointer

constexpr int raw_ptr_cast = 0; // either To or From is a raw pointer
constexpr int weak_ptr_downgrade_cast = 12; // To is weak, From is strong
constexpr int weak_ptr_upgrade_cast = 3;    // To is strong, From is weak
constexpr int neutral_cast = 6; // To and From are both weak or both strong

template <class T>
struct is_weak_ptr {
  static constexpr bool value = T::has_weak_ptr_semantics;
};

/// Convenience alias for `is_weak_ptr<T>::value`.
template <class T>
inline constexpr bool is_weak_ptr_v = is_weak_ptr<T>::value;

template <class T>
struct is_weak_ptr<T*> : std::false_type {};

template <class... Ts>
struct is_weak_ptr<typed_actor_pointer<Ts...>> : std::false_type {};

} // namespace

template <class To, class From, int>
class actor_cast_access;

template <class To, class From>
class actor_cast_access<To, From, raw_ptr_cast> {
public:
  To operator()(actor_control_block* x) const {
    return x;
  }

  To operator()(abstract_actor* x) const {
    return x->ctrl();
  }

  template <class T, class = std::enable_if_t<!std::is_pointer_v<T>>>
  To operator()(const T& x) const {
    return x.get();
  }
};

template <class To, class From>
class actor_cast_access<To*, From, raw_ptr_cast> {
public:
  To* operator()(actor_control_block* x) const {
    return static_cast<To*>(x->get());
  }

  To* operator()(abstract_actor* x) const {
    return static_cast<To*>(x);
  }

  template <class T, class = std::enable_if_t<!std::is_pointer_v<T>>>
  To* operator()(const T& x) const {
    return (*this)(x.get());
  }
};

template <class From>
class actor_cast_access<actor_control_block*, From, raw_ptr_cast> {
public:
  actor_control_block* operator()(actor_control_block* x) const {
    return x;
  }

  actor_control_block* operator()(abstract_actor* x) const {
    return x->ctrl();
  }

  template <class T, class = std::enable_if_t<!std::is_pointer_v<T>>>
  actor_control_block* operator()(const T& x) const {
    return x.get();
  }
};

template <class To, class From>
class actor_cast_access<To, From, weak_ptr_downgrade_cast> {
public:
  To operator()(const From& x) const {
    return x.get();
  }
};

template <class To, class From>
class actor_cast_access<To, From, weak_ptr_upgrade_cast> {
public:
  To operator()(const From& x) const {
    return {x.get_locked(), false};
  }
};

template <class To, class From>
class actor_cast_access<To, From, neutral_cast> {
public:
  To operator()(const From& x) const {
    return To{x.get()};
  }

  To operator()(From&& x) const {
    return {x.release(), false};
  }
};

/// Converts the actor handle `what` to a different actor handle or raw pointer
/// of type `T`.
template <class T, class U>
T actor_cast(U&& what) {
  // Should use remove_cvref in C++20.
  using from_type = std::remove_const_t<std::remove_reference_t<U>>;
  // query traits for T
  constexpr bool to_raw = std::is_pointer_v<T>;
  constexpr bool to_weak = is_weak_ptr_v<T>;
  // query traits for U
  constexpr bool from_raw = std::is_pointer_v<from_type>;
  constexpr bool from_weak = is_weak_ptr_v<from_type>;
  // calculate x and y
  constexpr int x = to_raw ? 0 : (to_weak ? 2 : 1);
  constexpr int y = from_raw ? 0 : (from_weak ? 3 : 6);
  // perform cast
  actor_cast_access<T, from_type, x * y> f;
  return f(std::forward<U>(what));
}

/// Converts the actor handle `what` to a different actor handle or raw pointer
/// of type `Tag::handle_type`.
template <class U, class Tag>
auto actor_cast(U&& what, Tag) {
  return actor_cast<typename Tag::handle_type>(std::forward<U>(what));
}

} // namespace caf
