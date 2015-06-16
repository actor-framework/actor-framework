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

#ifndef CAF_DETAIL_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
#define CAF_DETAIL_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP

#include <memory>

#include "caf/unit.hpp"
#include "caf/actor.hpp"
#include "caf/anything.hpp"
#include "caf/serializer.hpp"
#include "caf/typed_actor.hpp"
#include "caf/deserializer.hpp"
#include "caf/abstract_uniform_type_info.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

using uniform_type_info_ptr = uniform_type_info_ptr;

// check if there's a 'push_back' that takes a C::value_type
template <class T>
char sfinae_has_push_back(T* ptr, typename T::value_type* val = nullptr,
                          decltype(ptr->push_back(*val)) * = nullptr);

long sfinae_has_push_back(void*); // SFNINAE default

template <class T>
struct is_stl_compliant_list {
  static constexpr bool value =
    detail::is_iterable<T>::value
    && sizeof(sfinae_has_push_back(static_cast<T*>(nullptr))) == sizeof(char);
};

template<class T>
constexpr bool is_stl_compliant_list<T>::value;

// check if there's an 'insert' that takes a C::value_type
template <class T>
char sfinae_has_insert(T* ptr, typename T::value_type* val = nullptr,
                       decltype(ptr->insert(*val)) * = nullptr);

long sfinae_has_insert(void*); // SFNINAE default

template <class T>
struct is_stl_compliant_map {
  static constexpr bool value =
    detail::is_iterable<T>::value
    && sizeof(sfinae_has_insert(static_cast<T*>(nullptr))) == sizeof(char);
};

template <class T>
constexpr bool is_stl_compliant_map<T>::value;

template <class T>
struct is_stl_pair : std::false_type {
  // no members
};

template <class F, typename S>
struct is_stl_pair<std::pair<F, S>> : std::true_type {
  // no members
};

using primitive_impl = std::integral_constant<int, 0>;
using list_impl = std::integral_constant<int, 1>;
using map_impl = std::integral_constant<int, 2>;
using pair_impl = std::integral_constant<int, 3>;
using opt_impl = std::integral_constant<int, 4>;
using recursive_impl = std::integral_constant<int, 9>;

template <class T>
constexpr int impl_id() {
  return detail::is_primitive<T>::value
           ? 0
           : (is_stl_compliant_list<T>::value
                ? 1
                : (is_stl_compliant_map<T>::value
                     ? 2
                     : (is_stl_pair<T>::value
                          ? 3
                          : (detail::is_optional<T>::value
                              ? 4
                              : 9))));
}

template <class T>
struct deconst_pair {
  using type = T;
};

template <class K, typename V>
struct deconst_pair<std::pair<K, V>> {
  using first_type = typename std::remove_const<K>::type;
  using second_type = typename std::remove_const<V>::type;
  using type = std::pair<first_type, second_type>;
};

class default_serialize_policy {
public:
  template <class T>
  void operator()(const T& val, serializer* s) const {
    std::integral_constant<int, impl_id<T>()> token;
    simpl(val, s, token);
  }

  template <class T>
  void operator()(T& val, deserializer* d) const {
    std::integral_constant<int, impl_id<T>()> token;
    dimpl(val, d, token);
  }

private:
  template <class T>
  void simpl(const T& val, serializer* s, primitive_impl) const {
    s->write_value(val);
  }

  template <class T>
  void simpl(const T& val, serializer* s, list_impl) const {
    s->begin_sequence(val.size());
    for (auto i = val.begin(); i != val.end(); ++i) {
      (*this)(*i, s);
    }
    s->end_sequence();
  }

  template <class T>
  void simpl(const T& val, serializer* s, map_impl) const {
    // lists and maps share code for serialization
    list_impl token;
    simpl(val, s, token);
  }

  template <class T>
  void simpl(const T& val, serializer* s, pair_impl) const {
    (*this)(val.first, s);
    (*this)(val.second, s);
  }

  template <class T>
  void simpl(const optional<T>& val, serializer* s, opt_impl) const {
    uint8_t flag = val ? 1 : 0;
    s->write_value(flag);
    if (val) {
      (*this)(*val, s);
    }
  }

  template <class T>
  void simpl(const T& val, serializer* s, recursive_impl) const {
    uniform_typeid<T>()->serialize(&val, s);
  }

  template <class T>
  void dimpl(T& storage, deserializer* d, primitive_impl) const {
    storage = d->read<T>();
  }

  template <class T>
  void dimpl(T& storage, deserializer* d, list_impl) const {
    using value_type = typename T::value_type;
    storage.clear();
    size_t size = d->begin_sequence();
    for (size_t i = 0; i < size; ++i) {
      value_type tmp;
      (*this)(tmp, d);
      storage.push_back(std::move(tmp));
    }
    d->end_sequence();
  }

  template <class T>
  void dimpl(T& storage, deserializer* d, map_impl) const {
    storage.clear();
    size_t size = d->begin_sequence();
    for (size_t i = 0; i < size; ++i) {
      typename deconst_pair<typename T::value_type>::type tmp;
      (*this)(tmp, d);
      storage.insert(tmp);
    }
    d->end_sequence();
  }

  template <class T>
  void dimpl(T& storage, deserializer* d, pair_impl) const {
    (*this)(storage.first, d);
    (*this)(storage.second, d);
  }

  template <class T>
  void dimpl(optional<T>& val, deserializer* d, opt_impl) const {
    auto flag = d->read<uint8_t>();
    if (flag != 0) {
      T tmp;
      (*this)(tmp, d);
      val = std::move(tmp);
    } else {
      val = none;
    }
  }

  template <class T>
  void dimpl(T& storage, deserializer* d, recursive_impl) const {
    uniform_typeid<T>()->deserialize(&storage, d);
  }
};

class forwarding_serialize_policy {
public:
  inline forwarding_serialize_policy(uniform_type_info_ptr uti)
      : uti_(std::move(uti)) {
    // nop
  }

  template <class T>
  void operator()(const T& val, serializer* s) const {
    uti_->serialize(&val, s);
  }

  template <class T>
  void operator()(T& val, deserializer* d) const {
    uti_->deserialize(&val, d);
  }

private:
  uniform_type_info_ptr uti_;
};

template <class T, class AccessPolicy,
          class SerializePolicy = default_serialize_policy,
          bool IsEnum = std::is_enum<T>::value,
          bool IsEmptyType = std::is_class<T>::value&& std::is_empty<T>::value>
class member_tinfo : public abstract_uniform_type_info<T> {
public:
  using super = abstract_uniform_type_info<T>;
  member_tinfo(AccessPolicy apol, SerializePolicy spol)
      : super("--member--"),
        apol_(std::move(apol)), spol_(std::move(spol)) {
    // nop
  }

  member_tinfo(AccessPolicy apol)
      : super("--member--"),
        apol_(std::move(apol)) {
    // nop
  }

  member_tinfo() : super("--member--") {
    // nop
  }

  void serialize(const void* vptr, serializer* s) const override {
    spol_(apol_(vptr), s);
  }

  void deserialize(void* vptr, deserializer* d) const override {
    std::integral_constant<bool, AccessPolicy::grants_mutable_access> token;
    ds(vptr, d, token);
  }

private:

  void ds(void* p, deserializer* d, std::true_type) const {
    spol_(apol_(p), d);
  }

  void ds(void* p, deserializer* d, std::false_type) const {
    T tmp;
    spol_(tmp, d);
    apol_(p, std::move(tmp));
  }

  AccessPolicy apol_;
  SerializePolicy spol_;

};

template <class T, class A, class S>
class member_tinfo<T, A, S, false, true>
    : public abstract_uniform_type_info<T> {
public:
  using super = abstract_uniform_type_info<T>;

  member_tinfo(const A&, const S&) : super("--member--") {
    // nop
  }

  member_tinfo(const A&) : super("--member--") {
    // nop
  }

  member_tinfo() : super("--member--") {
    // nop
  }

  void serialize(const void*, serializer*) const override {
    // nop
  }

  void deserialize(void*, deserializer*) const override {
    // nop
  }
};

template <class T, class AccessPolicy, class SerializePolicy>
class member_tinfo<T, AccessPolicy, SerializePolicy, true, false>
    : public abstract_uniform_type_info<T> {
public:
  using super = abstract_uniform_type_info<T>;
  using value_type = typename std::underlying_type<T>::type;

  member_tinfo(AccessPolicy apol, SerializePolicy spol)
      : super("--member--"),
        apol_(std::move(apol)), spol_(std::move(spol)) {
    // nop
  }

  member_tinfo(AccessPolicy apol)
      : super("--member--"),
        apol_(std::move(apol)) {
    // nop
  }

  member_tinfo() : super("--member--") {
    // nop
  }

  void serialize(const void* p, serializer* s) const override {
    auto val = apol_(p);
    spol_(static_cast<value_type>(val), s);
  }

  void deserialize(void* p, deserializer* d) const override {
    value_type tmp;
    spol_(tmp, d);
    apol_(p, static_cast<T>(tmp));
  }

private:
  AccessPolicy apol_;
  SerializePolicy spol_;
};

template <class T, class C>
class memptr_access_policy {
public:
  inline memptr_access_policy(T C::*memptr) : memptr_(memptr) {
    // nop
  }
  memptr_access_policy(const memptr_access_policy&) = default;
  memptr_access_policy& operator=(const memptr_access_policy&) = default;

  inline T& operator()(void* vptr) const {
    auto ptr = reinterpret_cast<C*>(vptr);
    return *ptr.*memptr_;
  }

  inline const T& operator()(const void* vptr) const {
    auto ptr = reinterpret_cast<const C*>(vptr);
    return *ptr.*memptr_;
  }

  template <class Arg>
  inline void operator()(void* vptr, Arg&& value) const {
    auto ptr = reinterpret_cast<C*>(vptr);
    (*ptr.*memptr_) = std::forward<Arg>(value);
  }

  static constexpr bool grants_mutable_access = true;

private:
  T C::*memptr_;
};

template <class C, typename GRes, typename SRes, typename SArg>
class getter_setter_access_policy {
public:

  using getter = GRes (C::*)() const;
  using setter = SRes (C::*)(SArg);

  getter_setter_access_policy(getter g, setter s) : get_(g), set_(s) {}

  inline GRes operator()(const void* vptr) const {
    auto ptr = reinterpret_cast<const C*>(vptr);
    return (*ptr.*get_)();
  }

  template <class Arg>
  inline void operator()(void* vptr, Arg&& value) const {
    auto ptr = reinterpret_cast<C*>(vptr);
    (*ptr.*set_)(std::forward<Arg>(value));
  }

  static constexpr bool grants_mutable_access = false;

private:
  getter get_;
  setter set_;
};

template <class T>
struct fake_access_policy {
  inline T& operator()(void* vptr) const {
    return *reinterpret_cast<T*>(vptr);
  }

  inline const T& operator()(const void* vptr) const {
    return *reinterpret_cast<const T*>(vptr);
  }

  template <class U>
  inline void operator()(void* vptr, U&& value) const {
    *reinterpret_cast<T*>(vptr) = std::forward<U>(value);
  }

  static constexpr bool grants_mutable_access = true;
};

template <class T, class C>
uniform_type_info_ptr new_member_tinfo(T C::*memptr) {
  using access_policy = memptr_access_policy<T, C>;
  using result_type = member_tinfo<T, access_policy>;
  return uniform_type_info_ptr(new result_type(memptr));
}

template <class T, class C>
uniform_type_info_ptr new_member_tinfo(T C::*memptr,
                                       uniform_type_info_ptr meminf) {
  using access_policy = memptr_access_policy<T, C>;
  using tinfo = member_tinfo<T, access_policy, forwarding_serialize_policy>;
  return uniform_type_info_ptr(new tinfo(memptr, std::move(meminf)));
}

template <class C, typename GRes, typename SRes, typename SArg>
uniform_type_info_ptr new_member_tinfo(GRes (C::*getter)() const,
                                       SRes (C::*setter)(SArg)) {
  using access_policy = getter_setter_access_policy<C, GRes, SRes, SArg>;
  using value_type = typename std::decay<GRes>::type;
  using result_type = member_tinfo<value_type, access_policy>;
  return uniform_type_info_ptr(
    new result_type(access_policy(getter, setter)));
}

template <class C, typename GRes, typename SRes, typename SArg>
uniform_type_info_ptr new_member_tinfo(GRes (C::*getter)() const,
                                       SRes (C::*setter)(SArg),
                     uniform_type_info_ptr meminf) {
  using access_policy = getter_setter_access_policy<C, GRes, SRes, SArg>;
  using value_type = typename std::decay<GRes>::type;
  using tinfo = member_tinfo<value_type, access_policy,
                             forwarding_serialize_policy>;
  return uniform_type_info_ptr(new tinfo(access_policy(getter, setter),
                                         std::move(meminf)));
}

template <class T>
class default_uniform_type_info : public abstract_uniform_type_info<T> {
public:
  using super = abstract_uniform_type_info<T>;

  template <class... Ts>
  default_uniform_type_info(std::string tname, Ts&&... xs)
      : super(std::move(tname)) {
    push_back(std::forward<Ts>(xs)...);
  }

  default_uniform_type_info(std::string tname) : super(std::move(tname)) {
    using result_type = member_tinfo<T, fake_access_policy<T>>;
    members_.push_back(uniform_type_info_ptr(new result_type));
  }

  void serialize(const void* obj, serializer* s) const override {
    // serialize each member
    for (auto& m : members_) {
      m->serialize(obj, s);
    }
  }

  void deserialize(void* obj, deserializer* d) const override {
    // deserialize each member
    for (auto& m : members_) {
      m->deserialize(obj, d);
    }
  }

protected:
  bool pod_mems_equals(const T& lhs, const T& rhs) const override {
    return pod_eq(lhs, rhs);
  }

private:
  template <class C>
  typename std::enable_if<std::is_pod<C>::value, bool>::type
  pod_eq(const C& lhs, const C& rhs) const {
    for (auto& member : members_) {
      if (! member->equals(&lhs, &rhs)) return false;
    }
    return true;
  }

  template <class C>
  typename std::enable_if<! std::is_pod<C>::value, bool>::type
  pod_eq(const C&, const C&) const {
    return false;
  }

  inline void push_back() {
    // terminate recursion
  }

  template <class R, class C, class... Ts>
  void push_back(R C::*memptr, Ts&&... xs) {
    members_.push_back(new_member_tinfo(memptr));
    push_back(std::forward<Ts>(xs)...);
  }

  // pr.first = member pointer
  // pr.second = meta object to handle pr.first
  template <class R, class C, class... Ts>
  void push_back(const std::pair<R C::*,
                                 abstract_uniform_type_info<R>*>& pr,
                 Ts&&... xs) {
    members_.push_back(new_member_tinfo(pr.first,
                                         uniform_type_info_ptr(pr.second)));
    push_back(std::forward<Ts>(xs)...);
  }

  // pr.first = const-qualified getter
  // pr.second = setter with one argument
  template <class GR, typename SR, typename ST, typename C, class... Ts>
  void push_back(const std::pair<GR (C::*)() const,
                                 SR (C::*)(ST)>& pr,
                 Ts&&... xs) {
    members_.push_back(new_member_tinfo(pr.first, pr.second));
    push_back(std::forward<Ts>(xs)...);
  }

  // pr.first = pair of const-qualified getter and setter with one argument
  // pr.second = uniform type info pointer
  template <class GR, typename SR, typename ST, typename C, class... Ts>
  void push_back(const std::pair<
                         std::pair<GR (C::*)() const,
                                   SR (C::*)(ST)>,
                         abstract_uniform_type_info<
                           typename std::decay<GR>::type>*
                       >& pr,
                 Ts&&... xs) {
    members_.push_back(new_member_tinfo(pr.first.first, pr.first.second,
                       uniform_type_info_ptr(pr.second)));
    push_back(std::forward<Ts>(xs)...);
  }

  std::vector<uniform_type_info_ptr> members_;
};

template <class... Sigs>
class default_uniform_type_info<typed_actor<Sigs...>> :
    public abstract_uniform_type_info<typed_actor<Sigs...>> {
public:
  using super = abstract_uniform_type_info<typed_actor<Sigs...>>;
  using handle_type = typed_actor<Sigs...>;

  default_uniform_type_info(std::string tname) : super(std::move(tname)) {
    sub_uti = uniform_typeid<actor>();
  }

  void serialize(const void* obj, serializer* s) const override {
    auto tmp = actor_cast<actor>(deref(obj).address());
    sub_uti->serialize(&tmp, s);
  }

  void deserialize(void* obj, deserializer* d) const override {
    actor tmp;
    sub_uti->deserialize(&tmp, d);
    deref(obj) = actor_cast<handle_type>(tmp);
  }

private:
  static handle_type& deref(void* ptr) {
    return *reinterpret_cast<handle_type*>(ptr);
  }
  static const handle_type& deref(const void* ptr) {
    return *reinterpret_cast<const handle_type*>(ptr);
  }
  const uniform_type_info* sub_uti;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
