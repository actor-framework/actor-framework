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
  "@addr",
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

inline bool operator==(const unit_t&, const unit_t&) {
  return true;
}

template <class T>
inline typename std::enable_if<detail::is_primitive<T>::value>::type
serialize_impl(const T& val, serializer* sink) {
  sink->write_value(val);
}

template <class T>
inline typename std::enable_if<detail::is_primitive<T>::value>::type
deserialize_impl(T& val, deserializer* source) {
  val = source->read<T>();
}

inline void serialize_impl(const unit_t&, serializer*) {
  // nop
}

inline void deserialize_impl(unit_t&, deserializer*) {
  // nop
}

void serialize_impl(const actor_addr& addr, serializer* sink) {
  auto ns = sink->get_namespace();
  if (!ns) {
    throw std::runtime_error(
      "unable to serialize actor_addr: "
      "no actor addressing defined");
  }
  ns->write(sink, addr);
}

void deserialize_impl(actor_addr& addr, deserializer* source) {
  auto ns = source->get_namespace();
  if (!ns) {
    throw std::runtime_error(
      "unable to deserialize actor_ptr: "
      "no actor addressing defined");
  }
  addr = ns->read(source);
}

void serialize_impl(const actor& ptr, serializer* sink) {
  serialize_impl(ptr.address(), sink);
}

void deserialize_impl(actor& ptr, deserializer* source) {
  actor_addr addr;
  deserialize_impl(addr, source);
  ptr = actor_cast<actor>(addr);
}

void serialize_impl(const group& gref, serializer* sink) {
  if (!gref) {
    CAF_LOGF_DEBUG("serialized an invalid group");
    // write an empty string as module name
    std::string empty_string;
    sink->write_value(empty_string);
  } else {
    sink->write_value(gref->module_name());
    gref->serialize(sink);
  }
}

void deserialize_impl(group& gref, deserializer* source) {
  auto modname = source->read<std::string>();
  if (modname.empty()) {
    gref = invalid_group;
  } else {
    gref = group::get_module(modname)->deserialize(source);
  }
}

void serialize_impl(const channel& chref, serializer* sink) {
  // channel is an abstract base class that's either an actor or a group
  // to indicate that, we write a flag first, that is
  //   0 if ptr == nullptr
  //   1 if ptr points to an actor
  //   2 if ptr points to a group
  uint8_t flag = 0;
  auto wr_nullptr = [&] { sink->write_value(flag); };
  if (!chref) {
    // invalid channel
    wr_nullptr();
  } else {
    // raw pointer
    auto rptr = actor_cast<abstract_channel*>(chref);
    // raw actor pointer
    abstract_actor_ptr aptr = dynamic_cast<abstract_actor*>(rptr);
    if (aptr != nullptr) {
      flag = 1;
      sink->write_value(flag);
      serialize_impl(actor_cast<actor>(aptr), sink);
    } else {
      // get raw group pointer and store it inside a group handle
      group tmp{dynamic_cast<abstract_group*>(rptr)};
      if (tmp) {
        flag = 2;
        sink->write_value(flag);
        serialize_impl(tmp, sink);
      } else {
        CAF_LOGF_ERROR("ptr is neither an actor nor a group");
        wr_nullptr();
      }
    }
  }
}

void deserialize_impl(channel& ptrref, deserializer* source) {
  auto flag = source->read<uint8_t>();
  switch (flag) {
    case 0: {
      ptrref = channel{}; // ptrref.reset();
      break;
    }
    case 1: {
      actor tmp;
      deserialize_impl(tmp, source);
      ptrref = actor_cast<channel>(tmp);
      break;
    }
    case 2: {
      group tmp;
      deserialize_impl(tmp, source);
      ptrref = tmp;
      break;
    }
    default: {
      CAF_LOGF_ERROR("invalid flag while deserializing 'channel'");
      throw std::runtime_error("invalid flag");
    }
  }
}

void serialize_impl(const message& tup, serializer* sink) {
  std::string dynamic_name; // used if tup holds an object_array
  // ttn can be nullptr even if tuple is not empty (in case of object_array)
  const std::string* ttn = tup.empty() ? nullptr : tup.tuple_type_names();
  const char* tname = ttn ? ttn->data() : (tup.empty() ? "@<>" : nullptr);
  if (!tname) {
    // tuple is not empty, i.e., we are dealing with an object array
    dynamic_name = detail::get_tuple_type_names(*tup.vals());
    tname = dynamic_name.c_str();
  }
  auto uti_map = detail::singletons::get_uniform_type_info_map();
  auto uti = uti_map->by_uniform_name(tname);
  if (uti == nullptr) {
    std::string err = "could not get uniform type info for \"";
    err += tname;
    err += "\"";
    CAF_LOGF_ERROR(err);
    throw std::runtime_error(err);
  }
  sink->begin_object(uti);
  for (size_t i = 0; i < tup.size(); ++i) {
    uniform_type_info::from(tup.uniform_name_at(i))->serialize(tup.at(i), sink);
  }
  sink->end_object();
}

void deserialize_impl(message& atref, deserializer* source) {
  auto uti = source->begin_object();
  auto uval = uti->create();
  // auto ptr = uti->new_instance();
  uti->deserialize(uval->val, source);
  source->end_object();
  atref = uti->as_message(uval->val);
}

void serialize_impl(const node_id& nid, serializer* sink) {
  sink->write_raw(nid.host_id().size(), nid.host_id().data());
  sink->write_value(nid.process_id());
}

void deserialize_impl(node_id& nid, deserializer* source) {
  node_id::host_id_type hid;
  source->read_raw(node_id::host_id_size, hid.data());
  auto pid = source->read<uint32_t>();
  auto is_zero = [](uint8_t value) { return value == 0; };
  if (pid == 0 && std::all_of(hid.begin(), hid.end(), is_zero)) {
    // invalid process information
    nid = invalid_node_id;
  } else {
    nid = node_id{pid, hid};
  }
}

inline void serialize_impl(const atom_value& val, serializer* sink) {
  sink->write_value(val);
}

inline void deserialize_impl(atom_value& val, deserializer* source) {
  val = source->read<atom_value>();
}

inline void serialize_impl(const duration& val, serializer* sink) {
  sink->write_value(static_cast<uint32_t>(val.unit));
  sink->write_value(val.count);
}

inline void deserialize_impl(duration& val, deserializer* source) {
  auto unit_val = source->read<uint32_t>();
  auto count_val = source->read<uint32_t>();
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
  val.count = count_val;
}

inline void serialize_impl(bool val, serializer* sink) {
  sink->write_value(static_cast<uint8_t>(val ? 1 : 0));
}

inline void deserialize_impl(bool& val, deserializer* source) {
  val = source->read<uint8_t>() != 0;
}

// exit_msg & down_msg have the same fields
template <class T>
typename std::enable_if<
  std::is_same<T, down_msg>::value
  || std::is_same<T, exit_msg>::value
  || std::is_same<T, sync_exited_msg>::value
>::type
serialize_impl(const T& dm, serializer* sink) {
  serialize_impl(dm.source, sink);
  sink->write_value(dm.reason);
}

// exit_msg & down_msg have the same fields
template <class T>
typename std::enable_if<
  std::is_same<T, down_msg>::value
  || std::is_same<T, exit_msg>::value
  || std::is_same<T, sync_exited_msg>::value
>::type
deserialize_impl(T& dm, deserializer* source) {
  deserialize_impl(dm.source, source);
  dm.reason = source->read<uint32_t>();
}

inline void serialize_impl(const group_down_msg& dm, serializer* sink) {
  serialize_impl(dm.source, sink);
}

inline void deserialize_impl(group_down_msg& dm, deserializer* source) {
  deserialize_impl(dm.source, source);
}

inline void serialize_impl(const message_id& dm, serializer* sink) {
  sink->write_value(dm.integer_value());
}

inline void deserialize_impl(message_id& dm, deserializer* source) {
  dm = message_id::from_integer_value(source->read<uint64_t>());
}

inline void serialize_impl(const timeout_msg& tm, serializer* sink) {
  sink->write_value(tm.timeout_id);
}

inline void deserialize_impl(timeout_msg& tm, deserializer* source) {
  tm.timeout_id = source->read<uint32_t>();
}

inline void serialize_impl(const sync_timeout_msg&, serializer*) {
  // nop
}

inline void deserialize_impl(const sync_timeout_msg&, deserializer*) {
  // nop
}

template <class T>
typename std::enable_if<is_iterable<T>::value>::type
serialize_impl(const T& iterable, serializer* sink) {
  default_serialize_policy sp;
  sp(iterable, sink);
}

template <class T>
typename std::enable_if<is_iterable<T>::value>::type
deserialize_impl(T& iterable, deserializer* sink) {
  default_serialize_policy sp;
  sp(iterable, sink);
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

  message as_message(void* instance) const override {
    return make_message(deref(instance));
  }

  static inline const T& deref(const void* ptr) {
    return *reinterpret_cast<const T*>(ptr);
  }

  static inline T& deref(void* ptr) {
    return *reinterpret_cast<T*>(ptr);
  }

  void serialize(const void* instance, serializer* sink) const override {
    serialize_impl(deref(instance), sink);
  }

  void deserialize(void* instance, deserializer* source) const override {
    deserialize_impl(deref(instance), source);
  }

 private:
  template <class U>
  typename std::enable_if<std::is_floating_point<U>::value, bool>::type
  eq(const U& lhs, const U& rhs) const {
    return detail::safe_equal(lhs, rhs);
  }

  template <class U>
  typename std::enable_if<!std::is_floating_point<U>::value, bool>::type
  eq(const U& lhs, const U& rhs) const {
    return lhs == rhs;
  }
};

template <class T>
class int_tinfo : public uniform_type_info {
 public:
  int_tinfo() : uniform_type_info(detail::type_nr<T>::value) {
    // nop
  }
  void serialize(const void* instance, serializer* sink) const override {
    sink->write_value(deref(instance));
  }
  void deserialize(void* instance, deserializer* source) const override {
    deref(instance) = source->read<T>();
  }
  const char* name() const override {
    return static_name();
  }
  message as_message(void* instance) const override {
    return make_message(deref(instance));
  }

protected:
  bool equals(const void* lhs, const void* rhs) const override {
    return deref(lhs) == deref(rhs);
  }
  uniform_value create(const uniform_value& other) const override {
    return create_impl<T>(other);
  }
 private:
  inline static const T& deref(const void* ptr) {
    return *reinterpret_cast<const T*>(ptr);
  }
  inline static T& deref(void* ptr) {
    return *reinterpret_cast<T*>(ptr);
  }
  static inline const char* static_name() {
    return mapped_int_name<T>();
  }
};

class default_meta_message : public uniform_type_info {
 public:
  default_meta_message(const std::string& tname) {
    m_name = tname;
    std::vector<std::string> elements;
    split(elements, tname, is_any_of("+"));
    auto uti_map = detail::singletons::get_uniform_type_info_map();
    CAF_REQUIRE(elements.size() > 0 && elements.front() == "@<>");
    // ignore first element, because it's always "@<>"
    for (size_t i = 1; i != elements.size(); ++i) {
      try {
        m_elements.push_back(uti_map->by_uniform_name(elements[i]));
      }
      catch (std::exception&) {
        CAF_LOG_ERROR("type name " << elements[i] << " not found");
      }
    }
  }
  uniform_value create(const uniform_value& other) const override {
    auto res = create_impl<message>(other);
    if (!other) {
      // res is not a copy => fill with values
      message_builder mb;
      for (auto& e : m_elements) mb.append(e->create());
      *cast(res->val) = mb.to_message();
    }
    return res;
  }
  message as_message(void* ptr) const override {
    return *cast(ptr);
  }
  const char* name() const override {
    return m_name.c_str();
  }
  void serialize(const void* ptr, serializer* sink) const override {
    auto& msg = *cast(ptr);
    CAF_REQUIRE(msg.size() == m_elements.size());
    for (size_t i = 0; i < m_elements.size(); ++i) {
      m_elements[i]->serialize(msg.at(i), sink);
    }
  }
  void deserialize(void* ptr, deserializer* source) const override {
    message_builder mb;
    for (size_t i = 0; i < m_elements.size(); ++i) {
      mb.append(m_elements[i]->deserialize(source));
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
  std::string m_name;
  std::vector<const uniform_type_info*> m_elements;
};

template <class Iterator>
struct builtin_types_helper {
  Iterator pos;
  builtin_types_helper(Iterator first) : pos(first) {}
  inline void operator()() {
    // end of recursion
  }
  template <class T, class... Ts>
  inline void operator()(T& arg, Ts&... args) {
    *pos++ = &arg;
    (*this)(args...);
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
  /*
    // maps sizeof(integer_type) to {signed_type, unsigned_type}
    constexpr auto u8t  = tl_find<builtin_types, int_tinfo<uint8_t>>::value;
    constexpr auto i8t  = tl_find<builtin_types, int_tinfo<int8_t>>::value;
    constexpr auto u16t = tl_find<builtin_types, int_tinfo<uint16_t>>::value;
    constexpr auto i16t = tl_find<builtin_types, int_tinfo<int16_t>>::value;
    constexpr auto u32t = tl_find<builtin_types, int_tinfo<uint32_t>>::value;
    constexpr auto i32t = tl_find<builtin_types, int_tinfo<int32_t>>::value;
    constexpr auto u64t = tl_find<builtin_types, int_tinfo<uint64_t>>::value;
    constexpr auto i64t = tl_find<builtin_types, int_tinfo<int64_t>>::value;
    abstract_int_tinfo* mapping[][2] = {
      {nullptr, nullptr}, // no integer type for sizeof(T) == 0
      {&get<u8t>(m_storage), &get<i8t>(m_storage)},
      {&get<u16t>(m_storage), &get<i16t>(m_storage)},
      {nullptr, nullptr}, // no integer type for sizeof(T) == 3
      {&get<u32t>(m_storage), &get<i32t>(m_storage)},
      {nullptr, nullptr}, // no integer type for sizeof(T) == 5
      {nullptr, nullptr}, // no integer type for sizeof(T) == 6
      {nullptr, nullptr}, // no integer type for sizeof(T) == 7
      {&get<u64t>(m_storage), &get<i64t>(m_storage)}
    };
    push_native_type<char, signed char, unsigned char, short, signed short,
                     unsigned short, short int, signed short int,
                     unsigned short int, int, signed int, unsigned int,
                     long int, signed long int, unsigned long int, long,
                     signed long, unsigned long, long long, signed long long,
                     unsigned long long, wchar_t, int8_t, uint8_t, int16_t,
                     uint16_t, int32_t, uint32_t, int64_t, uint64_t, char16_t,
                     char32_t, size_t, ptrdiff_t, intptr_t>(mapping);
  */
    fill_uti_arr(std::integral_constant<size_t, 0>{},
                 m_builtin_types, m_storage);
    // make sure our builtin types are sorted
    CAF_REQUIRE(std::is_sorted(m_builtin_types.begin(),
                               m_builtin_types.end(),
                               [](pointer lhs, pointer rhs) {
                                 return strcmp(lhs->name(), rhs->name()) < 0;
                               }));
  }

  virtual pointer by_type_nr(uint16_t nr) const {
    CAF_REQUIRE(nr > 0);
    return m_builtin_types[nr - 1];
  }

  pointer by_rtti(const std::type_info& ti) const {
    shared_lock<detail::shared_spinlock> guard(m_lock);
    return find_rtti(ti);
  }

  pointer by_uniform_name(const std::string& name) {
    pointer result = find_name(m_builtin_types, name);
    if (!result) {
      shared_lock<detail::shared_spinlock> guard(m_lock);
      result = find_name(m_user_types, name);
    }
    if (!result && name.compare(0, 3, "@<>") == 0) {
      // create tuple UTI on-the-fly
      result = insert(nullptr, uniform_type_info_ptr{new default_meta_message(name)});
    }
    return result;
  }

  std::vector<pointer> get_all() const {
    shared_lock<detail::shared_spinlock> guard(m_lock);
    std::vector<pointer> res;
    res.reserve(m_builtin_types.size() + m_user_types.size());
    res.insert(res.end(), m_builtin_types.begin(), m_builtin_types.end());
    res.insert(res.end(), m_user_types.begin(), m_user_types.end());
    return res;
  }

  pointer insert(const std::type_info* ti, uniform_type_info_ptr uti) {
    unique_lock<detail::shared_spinlock> guard(m_lock);
    auto e = m_user_types.end();
    auto i = std::lower_bound(m_user_types.begin(), e, uti.get(),
                              [](pointer lhs, pointer rhs) {
      return strcmp(lhs->name(), rhs->name()) < 0;
    });
    if (i == e) {
      m_user_types.push_back(enriched_pointer{uti.release(), ti});
      return m_user_types.back();
    } else {
      if (strcmp(uti->name(), (*i)->name()) == 0) {
        // type already known
        return *i;
      }
      // insert after lower bound (vector is always sorted)
      auto new_pos = std::distance(m_user_types.begin(), i);
      m_user_types.insert(i, enriched_pointer{uti.release(), ti});
      return m_user_types[static_cast<size_t>(new_pos)];
    }
  }

  ~utim_impl() {
    for (auto ptr : m_user_types) {
      delete ptr;
    }
    m_user_types.clear();
  }

 private:
  using strmap = std::map<std::string, std::string>;

  using charbuf = std::vector<char>;

  using strvec = std::vector<std::string>;

  using builtin_types = std::tuple<uti_impl<actor>,
                                   uti_impl<actor_addr>,
                                   uti_impl<atom_value>,
                                   uti_impl<channel>,
                                   uti_impl<charbuf>,
                                   uti_impl<down_msg>,
                                   uti_impl<duration>,
                                   uti_impl<exit_msg>,
                                   uti_impl<group>,
                                   uti_impl<group_down_msg>,
                                   uti_impl<int16_t>,
                                   uti_impl<int32_t>,
                                   uti_impl<int64_t>,
                                   uti_impl<int8_t>,
                                   uti_impl<long double>,
                                   uti_impl<message>,
                                   uti_impl<message_id>,
                                   uti_impl<node_id>,
                                   uti_impl<std::string>,
                                   uti_impl<strmap>,
                                   uti_impl<std::set<std::string>>,
                                   uti_impl<strvec>,
                                   uti_impl<sync_exited_msg>,
                                   uti_impl<sync_timeout_msg>,
                                   uti_impl<timeout_msg>,
                                   uti_impl<uint16_t>,
                                   uti_impl<std::u16string>,
                                   uti_impl<uint32_t>,
                                   uti_impl<std::u32string>,
                                   uti_impl<uint64_t>,
                                   uti_impl<uint8_t>,
                                   uti_impl<unit_t>,
                                   uti_impl<bool>,
                                   uti_impl<double>,
                                   uti_impl<float>>;

  builtin_types m_storage;

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

  // bot containers are sorted by uniform name (m_user_types: second->name())
  std::array<pointer, std::tuple_size<builtin_types>::value> m_builtin_types;
  std::vector<enriched_pointer> m_user_types;
  mutable detail::shared_spinlock m_lock;

  pointer find_rtti(const std::type_info& ti) const {
    for (auto& utype : m_user_types) {
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
    return (i != e && name == (*i)->name()) ? *i : nullptr;
  }

};

} // namespace <anonymous>

uniform_type_info_map* uniform_type_info_map::create_singleton() {
  return new utim_impl;
}

uniform_type_info_map::~uniform_type_info_map() {
  // nop
}

} // namespace detail
} // namespace caf
