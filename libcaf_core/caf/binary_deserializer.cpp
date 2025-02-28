// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/binary_deserializer.hpp"

#include "caf/actor_system.hpp"
#include "caf/detail/ieee_754.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/error.hpp"
#include "caf/internal/fast_pimpl.hpp"
#include "caf/sec.hpp"

#include <sstream>
#include <type_traits>

namespace caf {

namespace {

template <class T>
constexpr size_t max_value = static_cast<size_t>(std::numeric_limits<T>::max());

class impl : public load_inspector_base<impl>,
             public internal::fast_pimpl<impl> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  impl(const std::byte* buf, size_t size, actor_system* sys = nullptr) noexcept
    : current_(buf), end_(buf + size), context_(sys) {
    // nop
  }

  // -- properties -------------------------------------------------------------

  size_t remaining() const noexcept {
    return static_cast<size_t>(end_ - current_);
  }

  const_byte_span remainder() const noexcept {
    return make_span(current_, end_);
  }

  actor_system* context() const noexcept {
    return context_;
  }

  void skip(size_t num_bytes) {
    if (num_bytes > remaining())
      CAF_RAISE_ERROR("cannot skip past the end");
    current_ += num_bytes;
  }

  void reset(const_byte_span bytes) noexcept {
    current_ = bytes.data();
    end_ = current_ + bytes.size();
  }

  const std::byte* current() const noexcept {
    return current_;
  }

  const std::byte* end() const noexcept {
    return end_;
  }

  static constexpr bool has_human_readable_format() noexcept {
    return false;
  }

  // -- overridden member functions --------------------------------------------

  void set_error(error stop_reason) {
    err_ = std::move(stop_reason);
  }

  error& get_error() noexcept {
    return err_;
  }

  bool fetch_next_object_type(type_id_t& type) noexcept {
    type = invalid_type_id;
    emplace_error(sec::unsupported_operation,
                  "the default binary format does not embed type information");
    return false;
  }

  constexpr bool begin_object(type_id_t, std::string_view) noexcept {
    return true;
  }

  constexpr bool end_object() noexcept {
    return true;
  }

  constexpr bool begin_field(std::string_view) noexcept {
    return true;
  }

  bool begin_field(std::string_view, bool& is_present) noexcept {
    auto tmp = uint8_t{0};
    if (!value(tmp))
      return false;
    is_present = static_cast<bool>(tmp);
    return true;
  }

  bool begin_field(std::string_view, span<const type_id_t> types,
                   size_t& index) noexcept {
    auto f = [&](auto tmp) {
      if (!value(tmp))
        return false;
      if (tmp < 0 || static_cast<size_t>(tmp) >= types.size()) {
        emplace_error(sec::invalid_field_type,
                      "received type index out of bounds");
        return false;
      }
      index = static_cast<size_t>(tmp);
      return true;
    };
    if (types.size() < max_value<int8_t>) {
      return f(int8_t{0});
    } else if (types.size() < max_value<int16_t>) {
      return f(int16_t{0});
    } else if (types.size() < max_value<int32_t>) {
      return f(int32_t{0});
    } else {
      return f(int64_t{0});
    }
  }

  bool begin_field(std::string_view, bool& is_present,
                   span<const type_id_t> types, size_t& index) noexcept {
    auto f = [&](auto tmp) {
      if (!value(tmp))
        return false;
      if (tmp < 0) {
        is_present = false;
        return true;
      }
      if (static_cast<size_t>(tmp) >= types.size()) {
        emplace_error(sec::invalid_field_type,
                      "received type index out of bounds");
        return false;
      }
      is_present = true;
      index = static_cast<size_t>(tmp);
      return true;
    };
    if (types.size() < max_value<int8_t>) {
      return f(int8_t{0});
    } else if (types.size() < max_value<int16_t>) {
      return f(int16_t{0});
    } else if (types.size() < max_value<int32_t>) {
      return f(int32_t{0});
    } else {
      return f(int64_t{0});
    }
  }
  constexpr bool end_field() {
    return true;
  }

  constexpr bool begin_tuple(size_t) noexcept {
    return true;
  }

  constexpr bool end_tuple() noexcept {
    return true;
  }

  constexpr bool begin_key_value_pair() noexcept {
    return true;
  }

  constexpr bool end_key_value_pair() noexcept {
    return true;
  }

  bool begin_sequence(size_t& list_size) noexcept {
    // Use varbyte encoding to compress sequence size on the wire.
    uint32_t x = 0;
    int n = 0;
    uint8_t low7 = 0;
    do {
      if (!value(low7))
        return false;
      x |= static_cast<uint32_t>((low7 & 0x7F)) << (7 * n);
      ++n;
    } while (low7 & 0x80);
    list_size = x;
    return true;
  }

  constexpr bool end_sequence() noexcept {
    return true;
  }

  bool begin_associative_array(size_t& size) noexcept {
    return begin_sequence(size);
  }

  bool end_associative_array() noexcept {
    return end_sequence();
  }

  bool value(bool& x) noexcept {
    int8_t tmp = 0;
    if (!value(tmp))
      return false;
    x = tmp != 0;
    return true;
  }

  bool value(std::byte& x) noexcept {
    if (range_check(1)) {
      x = *current_++;
      return true;
    }
    emplace_error(sec::end_of_stream);
    return false;
  }

  bool value(int8_t& x) noexcept {
    if (range_check(1)) {
      x = static_cast<int8_t>(*current_++);
      return true;
    }
    emplace_error(sec::end_of_stream);
    return false;
  }

  bool value(uint8_t& x) noexcept {
    if (range_check(1)) {
      x = static_cast<uint8_t>(*current_++);
      return true;
    }
    emplace_error(sec::end_of_stream);
    return false;
  }

  bool value(int16_t& x) noexcept {
    return int_value(x);
  }

  bool value(uint16_t& x) noexcept {
    return int_value(x);
  }

  bool value(int32_t& x) noexcept {
    return int_value(x);
  }

  bool value(uint32_t& x) noexcept {
    return int_value(x);
  }

  bool value(int64_t& x) noexcept {
    return int_value(x);
  }

  bool value(uint64_t& x) noexcept {
    return int_value(x);
  }

  bool value(float& x) noexcept {
    return float_value(x);
  }

  bool value(double& x) noexcept {
    return float_value(x);
  }

  bool value(long double& x) {
    // TODO: Our IEEE-754 conversion currently does not work for long double.
    // The
    //       standard does not guarantee a fixed representation for this type,
    //       but on X86 we can usually rely on 80-bit precision. For now, we
    //       fall back to string conversion.
    std::string tmp;
    if (!value(tmp))
      return false;
    std::istringstream iss{std::move(tmp)};
    if (iss >> x)
      return true;
    emplace_error(sec::invalid_argument);
    return false;
  }

  bool value(byte_span x) noexcept {
    if (!range_check(x.size())) {
      emplace_error(sec::end_of_stream);
      return false;
    }
    memcpy(x.data(), current_, x.size());
    current_ += x.size();
    return true;
  }

  bool value(std::string& x) {
    x.clear();
    size_t str_size = 0;
    if (!begin_sequence(str_size))
      return false;
    if (!range_check(str_size)) {
      emplace_error(sec::end_of_stream);
      return false;
    }
    x.assign(reinterpret_cast<const char*>(current_), str_size);
    current_ += str_size;
    return end_sequence();
  }

  bool value(std::u16string& x) {
    x.clear();
    size_t str_size = 0;
    if (!begin_sequence(str_size))
      return false;
    if (!range_check(str_size * sizeof(uint16_t))) {
      emplace_error(sec::end_of_stream);
      return false;
    }
    for (size_t i = 0; i < str_size; ++i) {
      // The standard does not guarantee that char16_t is exactly 16 bits.
      uint16_t tmp;
      unsafe_int_value(tmp);
      x.push_back(static_cast<char16_t>(tmp));
    }
    return end_sequence();
  }

  bool value(std::u32string& x) {
    x.clear();
    size_t str_size = 0;
    if (!begin_sequence(str_size))
      return false;
    if (!range_check(str_size * sizeof(uint32_t))) {
      emplace_error(sec::end_of_stream);
      return false;
    }
    for (size_t i = 0; i < str_size; ++i) {
      // The standard does not guarantee that char32_t is exactly 32 bits.
      uint32_t tmp;
      unsafe_int_value(tmp);
      x.push_back(static_cast<char32_t>(tmp));
    }
    return end_sequence();
  }

  bool value(std::vector<bool>& x) {
    x.clear();
    size_t len = 0;
    if (!begin_sequence(len))
      return false;
    if (len == 0)
      return end_sequence();
    size_t blocks = len / 8;
    for (size_t block = 0; block < blocks; ++block) {
      uint8_t tmp = 0;
      if (!value(tmp))
        return false;
      x.emplace_back((tmp & 0b1000'0000) != 0);
      x.emplace_back((tmp & 0b0100'0000) != 0);
      x.emplace_back((tmp & 0b0010'0000) != 0);
      x.emplace_back((tmp & 0b0001'0000) != 0);
      x.emplace_back((tmp & 0b0000'1000) != 0);
      x.emplace_back((tmp & 0b0000'0100) != 0);
      x.emplace_back((tmp & 0b0000'0010) != 0);
      x.emplace_back((tmp & 0b0000'0001) != 0);
    }
    auto trailing_block_size = len % 8;
    if (trailing_block_size > 0) {
      uint8_t tmp = 0;
      if (!value(tmp))
        return false;
      switch (trailing_block_size) {
        case 7:
          x.emplace_back((tmp & 0b0100'0000) != 0);
          [[fallthrough]];
        case 6:
          x.emplace_back((tmp & 0b0010'0000) != 0);
          [[fallthrough]];
        case 5:
          x.emplace_back((tmp & 0b0001'0000) != 0);
          [[fallthrough]];
        case 4:
          x.emplace_back((tmp & 0b0000'1000) != 0);
          [[fallthrough]];
        case 3:
          x.emplace_back((tmp & 0b0000'0100) != 0);
          [[fallthrough]];
        case 2:
          x.emplace_back((tmp & 0b0000'0010) != 0);
          [[fallthrough]];
        case 1:
          x.emplace_back((tmp & 0b0000'0001) != 0);
          [[fallthrough]];
        default:
          break;
      }
    }
    return end_sequence();
  }

  bool value(strong_actor_ptr& ptr) {
    auto aid = actor_id{0};
    auto nid = node_id{};
    if (!value(aid)) {
      return false;
    }
    if (!inspect(*this, nid)) {
      return false;
    }
    if (aid == 0) {
      ptr = nullptr;
    } else if (auto err = load_actor(ptr, context_, aid, nid)) {
      set_error(err);
      return false;
    }
    return true;
  }

  bool value(weak_actor_ptr& ptr) {
    strong_actor_ptr tmp;
    if (!value(tmp)) {
      return false;
    }
    ptr.reset(tmp.get());
    return true;
  }

private:
  /// Checks whether we can read `read_size` more bytes.
  bool range_check(size_t read_size) const noexcept {
    return current_ + read_size <= end_;
  }

  template <class T>
  bool int_value(T& x) {
    auto tmp = std::make_unsigned_t<T>{};
    if (value(as_writable_bytes(make_span(&tmp, 1)))) {
      x = static_cast<T>(detail::from_network_order(tmp));
      return true;
    } else {
      return false;
    }
  }

  template <class T>
  bool float_value(T& x) {
    auto tmp = typename detail::ieee_754_trait<T>::packed_type{};
    if (int_value(tmp)) {
      x = detail::unpack754(tmp);
      return true;
    } else {
      return false;
    }
  }

  // Does not perform any range checks.
  template <class T>
  void unsafe_int_value(T& x) {
    std::make_unsigned_t<T> tmp;
    memcpy(&tmp, current(), sizeof(tmp));
    skip(sizeof(tmp));
    x = static_cast<T>(detail::from_network_order(tmp));
  }

  /// Points to the current read position.
  const std::byte* current_;

  /// Points to the end of the assigned memory block.
  const std::byte* end_;

  /// Provides access to the ::proxy_registry and to the ::actor_system.
  actor_system* context_;

  /// The last occurred error.
  error err_;
};

} // namespace

// -- constructors, destructors, and assignment operators --------------------

binary_deserializer::binary_deserializer(const_byte_span input) noexcept {
  impl::construct(impl_, input.data(), input.size());
}

binary_deserializer::binary_deserializer(actor_system& sys,
                                         const_byte_span input) noexcept {
  impl::construct(impl_, input.data(), input.size(), &sys);
}

binary_deserializer::binary_deserializer(const void* buf,
                                         size_t size) noexcept {
  impl::construct(impl_, reinterpret_cast<const std::byte*>(buf), size);
}

binary_deserializer::binary_deserializer(actor_system& sys, const void* buf,
                                         size_t size) noexcept {
  impl::construct(impl_, reinterpret_cast<const std::byte*>(buf), size, &sys);
}

binary_deserializer::~binary_deserializer() {
  impl::destruct(impl_);
}

// -- properties -------------------------------------------------------------

size_t binary_deserializer::remaining() const noexcept {
  return impl::cast(impl_).remaining();
}

const_byte_span binary_deserializer::remainder() const noexcept {
  return impl::cast(impl_).remainder();
}

actor_system* binary_deserializer::context() const noexcept {
  return impl::cast(impl_).context();
}

void binary_deserializer::skip(size_t num_bytes) {
  impl::cast(impl_).skip(num_bytes);
}

void binary_deserializer::reset(const_byte_span bytes) noexcept {
  impl::cast(impl_).reset(bytes);
}

const std::byte* binary_deserializer::current() const noexcept {
  return impl::cast(impl_).current();
}

const std::byte* binary_deserializer::end() const noexcept {
  return impl::cast(impl_).end();
}

// -- overridden member functions --------------------------------------------

void binary_deserializer::set_error(error stop_reason) {
  impl::cast(impl_).set_error(std::move(stop_reason));
}

error& binary_deserializer::get_error() noexcept {
  return impl::cast(impl_).get_error();
}

bool binary_deserializer::fetch_next_object_type(type_id_t& type) noexcept {
  return impl::cast(impl_).fetch_next_object_type(type);
}

bool binary_deserializer::begin_object(type_id_t type,
                                       std::string_view type_name) noexcept {
  return impl::cast(impl_).begin_object(type, type_name);
}

bool binary_deserializer::end_object() noexcept {
  return impl::cast(impl_).end_object();
}

bool binary_deserializer::begin_field(std::string_view type_name) noexcept {
  return impl::cast(impl_).begin_field(type_name);
}

bool binary_deserializer::begin_field(std::string_view type_name,
                                      bool& is_present) noexcept {
  return impl::cast(impl_).begin_field(type_name, is_present);
}

bool binary_deserializer::begin_field(std::string_view type_name,
                                      span<const type_id_t> types,
                                      size_t& index) noexcept {
  return impl::cast(impl_).begin_field(type_name, types, index);
}

bool binary_deserializer::begin_field(std::string_view type_name,
                                      bool& is_present,
                                      span<const type_id_t> types,
                                      size_t& index) noexcept {
  return impl::cast(impl_).begin_field(type_name, is_present, types, index);
}

bool binary_deserializer::end_field() {
  return impl::cast(impl_).end_field();
}

bool binary_deserializer::begin_tuple(size_t size) noexcept {
  return impl::cast(impl_).begin_tuple(size);
}

bool binary_deserializer::end_tuple() noexcept {
  return impl::cast(impl_).end_tuple();
}

bool binary_deserializer::begin_key_value_pair() noexcept {
  return impl::cast(impl_).begin_key_value_pair();
}

bool binary_deserializer::end_key_value_pair() noexcept {
  return impl::cast(impl_).end_key_value_pair();
}

bool binary_deserializer::begin_sequence(size_t& list_size) noexcept {
  return impl::cast(impl_).begin_sequence(list_size);
}

bool binary_deserializer::end_sequence() noexcept {
  return impl::cast(impl_).end_sequence();
}

bool binary_deserializer::begin_associative_array(size_t& size) noexcept {
  return impl::cast(impl_).begin_associative_array(size);
}

bool binary_deserializer::end_associative_array() noexcept {
  return impl::cast(impl_).end_associative_array();
}

bool binary_deserializer::value(bool& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(std::byte& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(int8_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(uint8_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(int16_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(uint16_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(int32_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(uint32_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(int64_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(uint64_t& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(float& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(double& x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(long double& x) {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(byte_span x) noexcept {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(std::string& x) {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(std::u16string& x) {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(std::u32string& x) {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(std::vector<bool>& x) {
  return impl::cast(impl_).value(x);
}

bool binary_deserializer::value(strong_actor_ptr& ptr) {
  return impl::cast(impl_).value(ptr);
}

bool binary_deserializer::value(weak_actor_ptr& ptr) {
  return impl::cast(impl_).value(ptr);
}

} // namespace caf
