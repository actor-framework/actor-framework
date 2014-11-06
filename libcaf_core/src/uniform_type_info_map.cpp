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
#include "caf/detail/safe_equal.hpp"
#include "caf/detail/singletons.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/shared_spinlock.hpp"
#include "caf/detail/uniform_type_info_map.hpp"
#include "caf/detail/default_uniform_type_info.hpp"

namespace caf {
namespace detail {

namespace {
const char* mapped_type_names[][2] = {
  {"bool",                  "bool"                  },
  {"caf::actor",            "@actor"                },
  {"caf::actor_addr",       "@addr"                 },
  {"caf::atom_value",       "@atom"                 },
  {"caf::channel",          "@channel"              },
  {"caf::down_msg",         "@down"                 },
  {"caf::duration",         "@duration"             },
  {"caf::exit_msg",         "@exit"                 },
  {"caf::group",            "@group"                },
  {"caf::group_down_msg",   "@group_down"           },
  {"caf::message",          "@message"              },
  {"caf::message_id",       "@message_id"           },
  {"caf::node_id",          "@node"                 },
  {"caf::sync_exited_msg",  "@sync_exited"          },
  {"caf::sync_timeout_msg", "@sync_timeout"         },
  {"caf::timeout_msg",      "@timeout"              },
  {"caf::unit_t",           "@unit"                 },
  {"double",                "double"                },
  {"float",                 "float"                 },
  {"long double",           "@ldouble"              },
  // std::string
  {"std::basic_string<@i8,std::char_traits<@i8>,std::allocator<@i8>>", "@str"},
  // std::u16string
  {"std::basic_string<@u16,std::char_traits<@u16>,std::allocator<@u16>>",
   "@u16str"},
  // std::u32string
  {"std::basic_string<@u32,std::char_traits<@u32>,std::allocator<@u32>>",
   "@u32str"},
  // std::map<std::string,std::string>
  {"std::map<@str,@str,std::less<@str>,"
   "std::allocator<std::pair<const @str,@str>>>",
   "@strmap"},
  // std::vector<char>
  {"std::vector<@i8,std::allocator<@i8>>", "@charbuf"},
  // std::vector<std::string>
  {"std::vector<@str,std::allocator<@str>>", "@strvec"}
};
// the order of this table must be *identical* to mapped_type_names
using static_type_table = type_list<bool,
                                    actor,
                                    actor_addr,
                                    atom_value,
                                    channel,
                                    down_msg,
                                    duration,
                                    exit_msg,
                                    group,
                                    group_down_msg,
                                    message,
                                    message_id,
                                    node_id,
                                    sync_exited_msg,
                                    sync_timeout_msg,
                                    timeout_msg,
                                    unit_t,
                                    double,
                                    float,
                                    long double,
                                    std::string,
                                    std::u16string,
                                    std::u32string,
                                    std::map<std::string, std::string>,
                                    std::vector<char>>;
} // namespace <anonymous>

template <class T>
constexpr const char* mapped_name() {
  return mapped_type_names[detail::tl_find<static_type_table, T>::value][1];
}

// maps sizeof(T) => {unsigned name, signed name}
/* extern */ const char* mapped_int_names[][2] = {
  {nullptr, nullptr}, // no int type with 0 bytes
  {"@u8", "@i8"},
  {"@u16", "@i16"},
  {nullptr, nullptr}, // no int type with 3 bytes
  {"@u32", "@i32"},
  {nullptr, nullptr}, // no int type with 5 bytes
  {nullptr, nullptr}, // no int type with 6 bytes
  {nullptr, nullptr}, // no int type with 7 bytes
  {"@u64", "@i64"}
};

const char* mapped_name_by_decorated_name(const char* cstr) {
  using namespace std;
  auto cmp = [&](const char** lhs) { return strcmp(lhs[0], cstr) == 0; };
  auto e = end(mapped_type_names);
  auto i = find_if(begin(mapped_type_names), e, cmp);
  if (i != e){
    return (*i)[1];
  }
  // for some reason, GCC returns "std::string" as RTTI type name
  // instead of std::basic_string<...>, this also affects clang-compiled
  // code when used with GCC's libstdc++
  if (strcmp("std::string", cstr) == 0) {
    return mapped_name<std::string>();
  }
  return cstr; // no mapping found
}

std::string mapped_name_by_decorated_name(std::string&& str) {
  auto res = mapped_name_by_decorated_name(str.c_str());
  if (res == str.c_str()) {
    return std::move(str);
  }
  return res;
}

namespace {

inline bool operator==(const unit_t&, const unit_t&) {
  return true;
}

template <class T>
struct type_token { };

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
    tup.type_at(i)->serialize(tup.at(i), sink);
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

bool types_equal(const std::type_info* lhs, const std::type_info* rhs) {
  // in some cases (when dealing with dynamic libraries),
  // address can be different although types are equal
  return lhs == rhs || *lhs == *rhs;
}

template <class T>
class uti_base : public uniform_type_info {
 protected:
  uti_base() : m_native(&typeid(T)) {
    // nop
  }
  bool equal_to(const std::type_info& ti) const override {
    return types_equal(m_native, &ti);
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
  const std::type_info* m_native;

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
class uti_impl : public uti_base<T> {
 public:
  using super = uti_base<T>;

  const char* name() const {
    return mapped_name<T>();
  }

 protected:
  void serialize(const void* instance, serializer* sink) const {
    serialize_impl(super::deref(instance), sink);
  }
  void deserialize(void* instance, deserializer* source) const {
    deserialize_impl(super::deref(instance), source);
  }
};

class abstract_int_tinfo : public uniform_type_info {
 public:
  void add_native_type(const std::type_info& ti) {
    // only push back if not already set
    auto predicate = [&](const std::type_info* ptr) { return ptr == &ti; };
    if (std::none_of(m_natives.begin(), m_natives.end(), predicate))
      m_natives.push_back(&ti);
  }

 protected:
  std::vector<const std::type_info*> m_natives;
};

// unfortunately, one integer type can be mapped to multiple types
template <class T>
class int_tinfo : public abstract_int_tinfo {
 public:
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
  bool equal_to(const std::type_info& ti) const override {
    auto tptr = &ti;
    return std::any_of(
      m_natives.begin(), m_natives.end(),
      [tptr](const std::type_info* ptr) { return types_equal(ptr, tptr); });
  }
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
  default_meta_message(const std::string& name) {
    m_name = name;
    std::vector<std::string> elements;
    split(elements, name, is_any_of("+"));
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
    auto& lhs = *cast(instance1);
    auto& rhs = *cast(instance2);
    full_eq_type cmp;
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), cmp);
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

template <class T>
void push_native_type(abstract_int_tinfo* m[][2]) {
  m[sizeof(T)][std::is_signed<T>::value ? 1 : 0]->add_native_type(typeid(T));
}

template <class T0, typename T1, class... Ts>
void push_native_type(abstract_int_tinfo* m[][2]) {
  push_native_type<T0>(m);
  push_native_type<T1, Ts...>(m);
}

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

class utim_impl : public uniform_type_info_map {

 public:
  void initialize() {
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
    // fill builtin types *in sorted order* (by uniform name)
    auto i = m_builtin_types.begin();
    builtin_types_helper<decltype(i)> h{i};
    auto indices = detail::get_indices(m_storage);
    detail::apply_args(h, indices, m_storage);
    // make sure our builtin types are sorted
    auto cmp = [](pointer lhs, pointer rhs) {
      return strcmp(lhs->name(), rhs->name()) < 0;
    };
    std::sort(m_builtin_types.begin(), m_builtin_types.end(), cmp);
  }

  pointer by_rtti(const std::type_info& ti) const {
    shared_lock<detail::shared_spinlock> guard(m_lock);
    auto res = find_rtti(m_builtin_types, ti);
    return (res) ? res : find_rtti(m_user_types, ti);
  }

  pointer by_uniform_name(const std::string& name) {
    pointer result = nullptr;
    { // lifetime scope of guard
      shared_lock<detail::shared_spinlock> guard(m_lock);
      result = find_name(m_builtin_types, name);
      result = (result) ? result : find_name(m_user_types, name);
    }
    if (!result && name.compare(0, 3, "@<>") == 0) {
      // create tuple UTI on-the-fly
      result = insert(uniform_type_info_ptr{new default_meta_message(name)});
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

  pointer insert(uniform_type_info_ptr uti) {
    unique_lock<detail::shared_spinlock> guard(m_lock);
    auto e = m_user_types.end();
    auto i = std::lower_bound(m_user_types.begin(), e, uti.get(),
                              [](uniform_type_info* lhs, pointer rhs) {
      return strcmp(lhs->name(), rhs->name()) < 0;
    });
    if (i == e) {
      m_user_types.push_back(uti.release());
      return m_user_types.back();
    } else {
      if (strcmp(uti->name(), (*i)->name()) == 0) {
        // type already known
        return *i;
      }
      // insert after lower bound (vector is always sorted)
      auto new_pos = std::distance(m_user_types.begin(), i);
      m_user_types.insert(i, uti.release());
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

  using builtin_types = std::tuple<uti_impl<node_id>,
                                   uti_impl<channel>,
                                   uti_impl<down_msg>,
                                   uti_impl<exit_msg>,
                                   uti_impl<actor>,
                                   uti_impl<actor_addr>,
                                   uti_impl<group>,
                                   uti_impl<group_down_msg>,
                                   uti_impl<message>,
                                   uti_impl<message_id>,
                                   uti_impl<duration>,
                                   uti_impl<sync_exited_msg>,
                                   uti_impl<sync_timeout_msg>,
                                   uti_impl<timeout_msg>,
                                   uti_impl<unit_t>,
                                   uti_impl<atom_value>,
                                   uti_impl<std::string>,
                                   uti_impl<std::u16string>,
                                   uti_impl<std::u32string>,
                                   default_uniform_type_info<strmap>,
                                   uti_impl<bool>,
                                   uti_impl<float>,
                                   uti_impl<double>,
                                   uti_impl<long double>,
                                   int_tinfo<int8_t>,
                                   int_tinfo<uint8_t>,
                                   int_tinfo<int16_t>,
                                   int_tinfo<uint16_t>,
                                   int_tinfo<int32_t>,
                                   int_tinfo<uint32_t>,
                                   int_tinfo<int64_t>,
                                   int_tinfo<uint64_t>,
                                   default_uniform_type_info<charbuf>,
                                   default_uniform_type_info<strvec>>;

  builtin_types m_storage;

  // both containers are sorted by uniform name
  std::array<pointer, std::tuple_size<builtin_types>::value> m_builtin_types;
  std::vector<uniform_type_info*> m_user_types;
  mutable detail::shared_spinlock m_lock;

  template <class Container>
  pointer find_rtti(const Container& c, const std::type_info& ti) const {
    auto e = c.end();
    auto i = std::find_if(c.begin(), e,
                [&](pointer p) { return p->equal_to(ti); });
    return (i == e) ? nullptr : *i;
  }

  template <class Container>
  pointer find_name(const Container& c, const std::string& name) const {
    auto e = c.end();
    // both containers are sorted
    auto i = std::lower_bound(
      c.begin(), e, name,
      [](pointer p, const std::string& n) { return p->name() < n; });
    return (i != e && (*i)->name() == name) ? *i : nullptr;
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
