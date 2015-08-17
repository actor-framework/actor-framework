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

#include <ios> // std::ios_base::failure
#include <array>
#include <tuple>
#include <limits>
#include <string>
#include <vector>
#include <cstring> // memcmp
#include <algorithm>
#include <type_traits>

#include "caf/locks.hpp"

#include "caf/string_algorithms.hpp"

#include "caf/group.hpp"
#include "caf/channel.hpp"
#include "caf/message.hpp"
#include "caf/announce.hpp"
#include "caf/duration.hpp"
#include "caf/actor_cast.hpp"
#include "caf/abstract_group.hpp"
#include "caf/actor_namespace.hpp"
#include "caf/message_builder.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/type_nr.hpp"
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/detail/uniform_type_info_map.hpp"
#include "caf/detail/default_uniform_type_info.hpp"

namespace caf {
namespace detail {

const char* numbered_type_names[] = {
  "@actor",
  "@actorvec",
  "@addr",
  "@addrvec",
  "@atom",
  "@channel",
  "@charbuf",
  "@down",
  "@duration",
  "@exit",
  "@group",
  "@group_down",
  "@i16",
  "@i32",
  "@i64",
  "@i8",
  "@ldouble",
  "@message",
  "@message_id",
  "@node",
  "@str",
  "@strmap",
  "@strset",
  "@strvec",
  "@sync_exited",
  "@sync_timeout",
  "@timeout",
  "@u16",
  "@u16str",
  "@u32",
  "@u32str",
  "@u64",
  "@u8",
  "@unit",
  "bool",
  "double",
  "float"
};

namespace {

// primitive types are handled by serializer/deserializer directly
template <class T, class U>
typename std::enable_if<
  detail::is_primitive<U>::value || std::is_same<U, atom_value>::value
>::type
process(T& io, U& x) {
  io & x;
}

template <class U>
auto process(serializer& sink, const U& x) -> decltype(x.serialize(sink)) {
  x.serialize(sink);
}

template <class U>
auto process(deserializer& source, U& x) -> decltype(x.deserialize(source)) {
  x.deserialize(source);
}

template <class T>
void process(T&, const unit_t&) {
  // nop
}

void process(serializer& sink, const actor_addr& addr, actor_namespace* ns) {
  ns->write(&sink, addr);
}

void process(deserializer& source, actor_addr& addr, actor_namespace* ns) {
  addr = ns->read(&source);
}

template <class T, class U>
typename std::enable_if<
  std::is_same<typename std::remove_const<U>::type, actor_addr>::value
>::type
process(T& io, U& addr) {
  auto ns = io.get_namespace();
  if (! ns)
    throw std::runtime_error("no actor namespace found");
  process(io, addr, ns);
}

void process(serializer& sink, const actor& hdl) {
  auto addr = hdl.address();
  process(sink, addr);
}

void process(deserializer& source, actor& hdl) {
  actor_addr addr;
  process(source, addr);
  hdl = actor_cast<actor>(addr);
}

void process(serializer& sink, const group& gref) {
  if (! gref) {
    CAF_LOGF_DEBUG("serialized an invalid group");
    // write an empty string as module name
    std::string empty_string;
    sink.write_value(empty_string);
  } else {
    sink.write_value(gref->module_name());
    gref->serialize(&sink);
  }
}

void process(deserializer& source, group& gref) {
  auto modname = source.read<std::string>();
  if (modname.empty()) {
    gref = invalid_group;
  } else {
    gref = group::get_module(modname)->deserialize(&source);
  }
}

void process(serializer& sink, const channel& chref) {
  // abstract_channel is an abstract base class that's either an actor
  // or a group; we prefix the serialized data using a flag:
  //   0 if ptr == nullptr
  //   1 if ptr points to an actor
  //   2 if ptr points to a group
  uint8_t flag = 0;
  auto wr_nullptr = [&] { sink.write_value(flag); };
  if (! chref) {
    // invalid channel
    wr_nullptr();
  } else {
    // raw pointer
    auto rptr = actor_cast<abstract_channel*>(chref);
    CAF_ASSERT(rptr->is_abstract_actor() || rptr->is_abstract_group());
    if (rptr->is_abstract_actor()) {
      // raw actor pointer
      abstract_actor_ptr aptr = static_cast<abstract_actor*>(rptr);
      flag = 1;
      sink.write_value(flag);
      process(sink, actor_cast<actor>(aptr));
    } else {
      // get raw group pointer and store it inside a group handle
      group tmp{static_cast<abstract_group*>(rptr)};
      flag = 2;
      sink.write_value(flag);
      process(sink, tmp);
    }
  }
}

void process(deserializer& source, channel& ptrref) {
  auto flag = source.read<uint8_t>();
  switch (flag) {
    case 0: {
      ptrref = channel{}; // ptrref.reset();
      break;
    }
    case 1: {
      actor tmp;
      process(source, tmp);
      ptrref = actor_cast<channel>(tmp);
      break;
    }
    case 2: {
      group tmp;
      process(source, tmp);
      ptrref = tmp;
      break;
    }
    default: {
      CAF_LOGF_ERROR("invalid flag while deserializing 'channel'");
      throw std::runtime_error("invalid flag");
    }
  }
}

inline void process(serializer& sink, const duration& val) {
  sink & static_cast<uint32_t>(val.unit);
  sink & val.count;
}

inline void process(deserializer& source, duration& val) {
  uint32_t unit_val;
  source & unit_val;
  source & val.count;
  switch (unit_val) {
    case 1:
      val.unit = time_unit::seconds;
      break;
    case 1000:
      val.unit = time_unit::milliseconds;
      break;
    case 1000000:
      val.unit = time_unit::microseconds;
      break;
    default:
      val.unit = time_unit::invalid;
      break;
  }
}

// exit_msg & down_msg have the same fields
template <class T, class U>
typename std::enable_if<
  std::is_same<typename std::remove_const<U>::type, down_msg>::value
  || std::is_same<typename std::remove_const<U>::type, exit_msg>::value
  || std::is_same<typename std::remove_const<U>::type, sync_exited_msg>::value
>::type
process(T& io, U& x) {
  process(io, x.source);
  io & x.reason;
}

template <class T, class U>
typename std::enable_if<
  std::is_same<typename std::remove_const<U>::type, group_down_msg>::value
>::type
process(T& io, U& x) {
  process(io, x.source);
}

void process(serializer& sink, const message_id& x) {
  sink.write_value(x.integer_value());
}

void process(deserializer& source, message_id& x) {
  x = message_id::from_integer_value(source.read<uint64_t>());
}

template <class T, class U>
typename std::enable_if<
  std::is_same<typename std::remove_const<U>::type, timeout_msg>::value
>::type
process(T& sink, U& x) {
  sink & x.timeout_id;
}

template <class T>
void process(T&, const sync_timeout_msg&) {
  // nop
}

template <class T, class U>
typename std::enable_if<
  is_iterable<typename std::remove_const<U>::type>::value
>::type
process(T& io, U& iterable) {
  default_serialize_policy sp;
  sp(iterable, &io);
}

template <class T>
class uti_impl : public uniform_type_info {
public:
  static_assert(detail::type_nr<T>::value > 0, "type_nr<T>::value == 0");

  uti_impl() : uniform_type_info(detail::type_nr<T>::value) {
    // nop
  }

  const char* name() const override {
    return numbered_type_names[detail::type_nr<T>::value - 1];
  }

  bool equal_to(const std::type_info&) const override {
    // CAF never compares builtin types using RTTI
    return false;
  }

  bool equals(const void* lhs, const void* rhs) const override {
    // return detail::safe_equal(deref(lhs), deref(rhs));
    // return deref(lhs) == deref(rhs);
    return eq(deref(lhs), deref(rhs));
  }

  uniform_value create(const uniform_value& other) const override {
    return create_impl<T>(other);
  }

  void serialize(const void* instance, serializer* sink) const override {
    process(*sink, deref(instance));
  }

  void deserialize(void* instance, deserializer* source) const override {
    process(*source, deref(instance));
  }

private:
  static inline const T& deref(const void* ptr) {
    return *reinterpret_cast<const T*>(ptr);
  }

  static inline T& deref(void* ptr) {
    return *reinterpret_cast<T*>(ptr);
  }

  template <class U>
  typename std::enable_if<std::is_floating_point<U>::value, bool>::type
  eq(const U& lhs, const U& rhs) const {
    return detail::safe_equal(lhs, rhs);
  }

  template <class U>
  typename std::enable_if<! std::is_floating_point<U>::value, bool>::type
  eq(const U& lhs, const U& rhs) const {
    return lhs == rhs;
  }
};

template <class T>
struct get_uti_impl {
  using type = uti_impl<T>;
};

class default_meta_message : public uniform_type_info {
public:
  explicit default_meta_message(const std::string& tname) : name_(tname) {
    std::vector<std::string> elements;
    split(elements, tname, is_any_of("+"));
    auto uti_map = detail::singletons::get_uniform_type_info_map();
    CAF_ASSERT(elements.size() > 0 && elements.front() == "@<>");
    // ignore first element, because it's always "@<>"
    for (size_t i = 1; i != elements.size(); ++i) {
      try {
        elements_.push_back(uti_map->by_uniform_name(elements[i]));
      }
      catch (std::exception&) {
        CAF_LOG_ERROR("type name " << elements[i] << " not found");
      }
    }
  }

  uniform_value create(const uniform_value& other) const override {
    auto res = create_impl<message>(other);
    if (! other) {
      // res is not a copy => fill with values
      message_builder mb;
      for (auto& e : elements_) mb.append(e->create());
      *cast(res->val) = mb.to_message();
    }
    return res;
  }

  const char* name() const override {
    return name_.c_str();
  }

  void serialize(const void* ptr, serializer* sink) const override {
    auto& msg = *cast(ptr);
    CAF_ASSERT(msg.size() == elements_.size());
    for (size_t i = 0; i < elements_.size(); ++i) {
      elements_[i]->serialize(msg.at(i), sink);
    }
  }

  void deserialize(void* ptr, deserializer* source) const override {
    message_builder mb;
    for (size_t i = 0; i < elements_.size(); ++i) {
      mb.append(elements_[i]->deserialize(source));
    }
    *cast(ptr) = mb.to_message();
  }

  bool equal_to(const std::type_info&) const override {
    return false;
  }

  bool equals(const void* instance1, const void* instance2) const override {
    return *cast(instance1) == *cast(instance2);
  }

private:
  inline message* cast(void* ptr) const {
    return reinterpret_cast<message*>(ptr);
  }
  inline const message* cast(const void* ptr) const {
    return reinterpret_cast<const message*>(ptr);
  }
  std::string name_;
  std::vector<const uniform_type_info*> elements_;
};

template <class Iterator>
struct builtin_types_helper {
  Iterator pos;
  explicit builtin_types_helper(Iterator first) : pos(first) {
    // nop
  }
  void operator()() const {
    // end of recursion
  }
  template <class T, class... Ts>
  void operator()(T& x, Ts&... xs) {
    *pos++ = &x;
    (*this)(xs...);
  }
};

constexpr size_t builtins = detail::type_nrs - 1;

using builtins_t = std::integral_constant<size_t, builtins>;

using type_array = std::array<const uniform_type_info*, builtins>;

template <class T>
void fill_uti_arr(builtins_t, type_array&, const T&) {
  // end of recursion
}

template <size_t N, class T>
void fill_uti_arr(std::integral_constant<size_t, N>, type_array& arr, const T& tup) {
  arr[N] = &std::get<N>(tup);
  fill_uti_arr(std::integral_constant<size_t, N+1>{}, arr, tup);
}

class utim_impl : public uniform_type_info_map {
public:
  void initialize() {
    fill_uti_arr(std::integral_constant<size_t, 0>{},
                 builtin_types_, storage_);
    // make sure our builtin types are sorted
    CAF_ASSERT(std::is_sorted(builtin_types_.begin(),
                               builtin_types_.end(),
                               [](pointer lhs, pointer rhs) {
                                 return strcmp(lhs->name(), rhs->name()) < 0;
                               }));
  }

  virtual pointer by_type_nr(uint16_t nr) const {
    CAF_ASSERT(nr > 0);
    return builtin_types_[nr - 1];
  }

  pointer by_rtti(const std::type_info& ti) const {
    shared_lock<detail::shared_spinlock> guard(lock_);
    return find_rtti(ti);
  }

  pointer by_uniform_name(const std::string& name) {
    pointer result = find_name(builtin_types_, name);
    if (! result) {
      shared_lock<detail::shared_spinlock> guard(lock_);
      result = find_name(user_types_, name);
    }
    if (! result && name.compare(0, 3, "@<>") == 0) {
      // create tuple UTI on-the-fly
      result = insert(nullptr, uniform_type_info_ptr{new default_meta_message(name)});
    }
    return result;
  }

  std::vector<pointer> get_all() const {
    shared_lock<detail::shared_spinlock> guard(lock_);
    std::vector<pointer> res;
    res.reserve(builtin_types_.size() + user_types_.size());
    res.insert(res.end(), builtin_types_.begin(), builtin_types_.end());
    res.insert(res.end(), user_types_.begin(), user_types_.end());
    return res;
  }

  pointer insert(const std::type_info* ti, uniform_type_info_ptr uti) {
    unique_lock<detail::shared_spinlock> guard(lock_);
    auto e = user_types_.end();
    auto i = std::lower_bound(user_types_.begin(), e, uti.get(),
                              [](pointer lhs, pointer rhs) {
      return strcmp(lhs->name(), rhs->name()) < 0;
    });
    if (i == e) {
      user_types_.push_back(enriched_pointer{uti.release(), ti});
      return user_types_.back();
    } else {
      if (strcmp(uti->name(), (*i)->name()) == 0) {
        // type already known
        return *i;
      }
      // insert after lower bound (vector is always sorted)
      auto new_pos = std::distance(user_types_.begin(), i);
      user_types_.insert(i, enriched_pointer{uti.release(), ti});
      return user_types_[static_cast<size_t>(new_pos)];
    }
  }

  ~utim_impl() {
    for (auto ptr : user_types_) {
      delete ptr;
    }
    user_types_.clear();
  }

private:
  using strmap = std::map<std::string, std::string>;

  using charbuf = std::vector<char>;

  using strvec = std::vector<std::string>;

  using builtin_types =
    tl_apply<
      tl_map<
        sorted_builtin_types,
        get_uti_impl
      >::type,
      std::tuple
    >::type;

  builtin_types storage_;

  struct enriched_pointer {
    pointer first;
    const std::type_info* second;
    operator pointer() const {
      return first;
    }
    pointer operator->() const {
      return first;
    }
  };

  using pointer_pair = std::pair<uniform_type_info*, std::type_info*>;

  // bot containers are sorted by uniform name (user_types_: second->name())
  std::array<pointer, type_nrs - 1> builtin_types_;
  std::vector<enriched_pointer> user_types_;
  mutable detail::shared_spinlock lock_;

  pointer find_rtti(const std::type_info& ti) const {
    for (auto& utype : user_types_) {
      if (utype.second && (utype.second == &ti || *utype.second == ti)) {
        return utype.first;
      }
    }
    return nullptr;
  }

  template <class Container>
  pointer find_name(const Container& c, const std::string& name) const {
    auto e = c.end();
    auto cmp = [](pointer lhs, const std::string& rhs) {
      return lhs->name() < rhs;
    };
    // both containers are sorted
    auto i = std::lower_bound(c.begin(), e, name, cmp);
    if (i != e && name == (*i)->name()) {
      return *i;
    }
    return nullptr;
  }
};

} // namespace <anonymous>

uniform_type_info_map* uniform_type_info_map::create_singleton() {
  return new utim_impl;
}

void uniform_type_info_map::dispose() {
  delete this;
}

void uniform_type_info_map::stop() {
  // nop
}

uniform_type_info_map::~uniform_type_info_map() {
  // nop
}

} // namespace detail
} // namespace caf
