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

#ifndef CAF_UNIFORM_TYPE_INFO_HPP
#define CAF_UNIFORM_TYPE_INFO_HPP

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <typeinfo>
#include <type_traits>

#include "caf/config.hpp"
#include "caf/uniform_typeid.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

class serializer;
class deserializer;
class uniform_type_info;

struct uniform_value_t;

/// A unique pointer holding a `uniform_value_t`.
using uniform_value = std::unique_ptr<uniform_value_t>;

/// Generic container for storing a value with associated type information.
struct uniform_value_t {
  const uniform_type_info* ti;
  void* val;
  uniform_value_t(const uniform_type_info*, void*);
  virtual uniform_value copy() = 0;
  virtual ~uniform_value_t();
};

template <class T>
class uniform_value_impl : public uniform_value_t {
public:
  template <class... Ts>
  uniform_value_impl(const uniform_type_info* ptr, Ts&&... xs)
      : uniform_value_t(ptr, &value_),
        value_(std::forward<Ts>(xs)...) {
    // nop
  }

  uniform_value copy() override {
    return uniform_value{new uniform_value_impl(ti, value_)};
  }

private:
  T value_;
};

/// Creates a uniform value of type `T`.
template <class T, class... Ts>
uniform_value make_uniform_value(const uniform_type_info* uti, Ts&&... xs) {
  return uniform_value{new uniform_value_impl<T>(uti, std::forward<Ts>(xs)...)};
}

/// @defgroup TypeSystem Platform-independent type system.
///
/// `libcaf` provides a fully network transparent communication between
/// actors. Thus, it needs to serialize and deserialize message objects.
/// Unfortunately, this is not possible using the C++ RTTI system.
///
/// Since it is not possible to extend `std::type_info, `libcaf`
/// uses its own type abstraction: `uniform_type_info`.
///
/// Unlike `std::type_info::name()`, `uniform_type_info::name()`
/// is guaranteed to return the same name on all supported platforms.
/// Furthermore, it allows to create an instance of a type by name:
///
/// @code
/// // creates a signed, 32 bit integer
/// uniform_value i = caf::uniform_type_info::by_name("@i32")->create();
/// @endcode
///
/// However, you should rarely if ever need to use `uniform_value`
/// or `uniform_type_info`.
///
/// There is one exception, though, where you need to care about
/// the type system: using custom data types in messages.
/// The source code below compiles fine, but crashes with an exception during
/// runtime.
///
/// ~~~
/// #include "caf/all.hpp"
/// using namespace caf;
///
/// struct foo { int a; int b; };
///
/// int main() {
///   send(self, foo{1, 2});
///   return 0;
/// }
/// ~~~
///
/// The user-defined struct `foo` is not known by the type system.
/// Thus, `foo` cannot be serialized and the code above code above will
/// throw a `std::runtime_error`.
///
/// Fortunately, there is an easy way to add simple data structures like
/// `foo` the type system without implementing serialize/deserialize yourself:
/// `caf::announce<foo>("foo", &foo::a, &foo::b)`.
///
/// The function `announce()` takes the class as template
/// parameter. The name of the announced type is the first argument, followed
/// by pointers to all members (or getter/setter pairs). This works for all
/// primitive data types and STL compliant containers. See the announce example
/// for more details. Complex data structures usually require a custom
/// serializer class.

/// @ingroup TypeSystem
/// Provides a platform independent type name and a (very primitive)
/// kind of reflection in combination with `uniform_value`.
/// CAF uses abbvreviate type names for brevity:
/// - std::string is named \@str
/// - std::u16string is named \@u16str
/// - std::u32string is named \@u32str
/// - integers are named `\@(i|u)$size\n
///   e.g.: \@i32 is a 32 bit signed integer; \@u16
///   is a 16 bit unsigned integer
class uniform_type_info {
public:
  friend bool operator==(const uniform_type_info& lhs,
                         const uniform_type_info& rhs);

  // disable copy and move constructors
  uniform_type_info(uniform_type_info&&) = delete;
  uniform_type_info(const uniform_type_info&) = delete;

  // disable assignment operators
  uniform_type_info& operator=(uniform_type_info&&) = delete;
  uniform_type_info& operator=(const uniform_type_info&) = delete;

  virtual ~uniform_type_info();

  /// Get instance by uniform name.
  /// @param uniform_name The internal name for a type.
  /// @returns The instance associated to `uniform_name`.
  /// @throws std::runtime_error if no type named `uniform_name` was found.
  static const uniform_type_info* from(const std::string& uniform_name);

  /// Get instance by std::type_info.
  /// @param tinfo A STL RTTI object.
  /// @returns An instance describing the same type as `tinfo`.
  /// @throws std::runtime_error if `tinfo` is not an announced type.
  static const uniform_type_info* from(const std::type_info& tinfo);

  /// Returns a vector with all known (announced) types.
  static std::vector<const uniform_type_info*> instances();

  /// Creates a copy of `other`.
  virtual uniform_value create(const uniform_value& other
                               = uniform_value{}) const = 0;

  /// Deserializes an object of this type from `source`.
  uniform_value deserialize(deserializer* source) const;

  /// Get the internal name for this type.
  /// @returns A string describing the internal type name.
  virtual const char* name() const = 0;

  /// Determines if this uniform_type_info describes the same
  ///    type than `tinfo`.
  /// @returns `true` if `tinfo` describes the same type as `this`.
  virtual bool equal_to(const std::type_info& tinfo) const = 0;

  /// Compares two instances of this type.
  /// @param instance1 Left hand operand.
  /// @param instance2 Right hand operand.
  /// @returns `true` if `*instance1 == *instance2.
  /// @pre `instance1` and `instance2` have the type of `this`.
  virtual bool equals(const void* instance1, const void* instance2) const = 0;

  /// Serializes `instance` to `sink`.
  /// @param instance Instance of this type.
  /// @param sink Target data sink.
  /// @pre `instance` has the type of `this`.
  /// @throws std::ios_base::failure Thrown when the underlying serialization
  ///                layer is unable to serialize the data,
  ///                e.g., when exceeding maximum buffer
  ///                sizes.
  virtual void serialize(const void* instance, serializer* sink) const = 0;

  /// Deserializes `instance` from `source`.
  /// @param instance Instance of this type.
  /// @param source Data source.
  /// @pre `instance` has the type of `this`.
  virtual void deserialize(void* instance, deserializer* source) const = 0;

  /// Returns `instance` encapsulated as an `message`.
  virtual message as_message(void* instance) const = 0;

  /// Returns a unique number for builtin types or 0.
  uint16_t type_nr() const {
    return type_nr_;
  }

protected:
  uniform_type_info(uint16_t typenr = 0);

  template <class T>
  uniform_value create_impl(const uniform_value& other) const {
    if (other) {
      CAF_ASSERT(other->ti == this);
      auto ptr = reinterpret_cast<const T*>(other->val);
      return make_uniform_value<T>(this, *ptr);
    }
    return make_uniform_value<T>(this);
  }

private:
  uint16_t type_nr_;
};

/// @relates uniform_type_info
using uniform_type_info_ptr = std::unique_ptr<uniform_type_info>;

/// @relates uniform_type_info
inline bool operator==(const uniform_type_info& lhs,
                       const uniform_type_info& rhs) {
  // uniform_type_info instances are singletons
  return &lhs == &rhs;
}

/// @relates uniform_type_info
inline bool operator!=(const uniform_type_info& lhs,
             const uniform_type_info& rhs) {
  return !(lhs == rhs);
}

} // namespace caf

#endif // CAF_UNIFORM_TYPE_INFO_HPP
