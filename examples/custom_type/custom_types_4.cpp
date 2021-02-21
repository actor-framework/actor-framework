// Showcases custom message types with a sealed class hierarchy.

#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

#include "caf/all.hpp"

class circle;
class shape;
class rectangle;

struct point;

using shape_ptr = std::shared_ptr<shape>;

CAF_BEGIN_TYPE_ID_BLOCK(custom_types_4, first_custom_type_id)

  CAF_ADD_TYPE_ID(custom_types_4, (circle))
  CAF_ADD_TYPE_ID(custom_types_4, (point))
  CAF_ADD_TYPE_ID(custom_types_4, (shape_ptr))
  CAF_ADD_TYPE_ID(custom_types_4, (rectangle))

CAF_END_TYPE_ID_BLOCK(custom_types_4)

struct point {
  int32_t x = 0;
  int32_t y = 0;
};

template <class Inspector>
bool inspect(Inspector& f, point& x) {
  return f.object(x).fields(f.field("x", x.x), f.field("y", x.y));
}

class shape {
public:
  friend class circle;
  friend class rectangle;

  virtual ~shape() {
    // nop
  }

  [[nodiscard]] virtual caf::type_id_t type() const noexcept = 0;

protected:
  shape() = default;
  shape(const shape&) = default;
  shape& operator=(const shape&) = default;
};

class rectangle final : public shape {
public:
  caf::type_id_t type() const noexcept override {
    return caf::type_id_v<rectangle>;
  }

  [[nodiscard]] point top_left() const noexcept {
    return top_left_;
  }

  [[nodiscard]] point bottom_right() const noexcept {
    return bottom_right_;
  }

  rectangle(point top_left, point bottom_right)
    : top_left_(top_left), bottom_right_(bottom_right) {
    // nop
  }

  rectangle() = default;

  rectangle(const rectangle&) = default;

  rectangle& operator=(const rectangle&) = default;

  template <class Inspector>
  friend bool inspect(Inspector& f, rectangle& x) {
    return f.object(x).fields(f.field("top-left", x.top_left_),
                              f.field("bottom-right", x.bottom_right_));
  }

  static auto make(point top_left, point bottom_right) {
    return std::make_shared<rectangle>(top_left, bottom_right);
  }

private:
  point top_left_;
  point bottom_right_;
};

class circle final : public shape {
public:
  caf::type_id_t type() const noexcept override {
    return caf::type_id_v<circle>;
  }

  [[nodiscard]] point center() const noexcept {
    return center_;
  }

  [[nodiscard]] int32_t radius() const noexcept {
    return radius_;
  }

  circle(point center, int32_t radius) : center_(center), radius_(radius) {
    // nop
  }

  circle() = default;

  circle(const circle&) = default;

  circle& operator=(const circle&) = default;

  template <class Inspector>
  friend bool inspect(Inspector& f, circle& x) {
    return f.object(x).fields(f.field("center", x.center_),
                              f.field("radius", x.radius_));
  }

  static auto make(point center, int32_t radius) {
    return std::make_shared<circle>(center, radius);
  }

private:
  point center_;
  int32_t radius_;
};

// Treat shape_ptr like a variant<none_t, rectangle, circle>, where none_t
// represents a default-constructed (nullptr) shape_ptr.
namespace caf {

template <>
struct variant_inspector_traits<shape_ptr> {
  using value_type = shape_ptr;

  // Lists all allowed types and gives them a 0-based index.
  static constexpr type_id_t allowed_types[] = {
    type_id_v<none_t>,
    type_id_v<rectangle>,
    type_id_v<circle>,
  };

  // Returns which type in allowed_types corresponds to x.
  static auto type_index(const value_type& x) {
    if (!x)
      return 0;
    else if (x->type() == type_id_v<rectangle>)
      return 1;
    else
      return 2;
  }

  // Applies f to the value of x.
  template <class F>
  static auto visit(F&& f, const value_type& x) {
    switch (type_index(x)) {
      case 0: {
        none_t dummy;
        return f(dummy);
      }
      case 1:
        return f(static_cast<rectangle&>(*x));
      default:
        return f(static_cast<circle&>(*x));
    }
  }

  // Assigns a value to x.
  template <class U>
  static void assign(value_type& x, U value) {
    if constexpr (std::is_same<U, none_t>::value)
      x.reset();
    else
      x = std::make_shared<U>(std::move(value));
  }

  // Create a default-constructed object for `type` and then call the
  // continuation with the temporary object to perform remaining load steps.
  template <class F>
  static bool load(type_id_t type, F continuation) {
    switch (type) {
      default:
        return false;
      case type_id_v<none_t>: {
        none_t dummy;
        continuation(dummy);
        return true;
      }
      case type_id_v<rectangle>: {
        auto tmp = rectangle{};
        continuation(tmp);
        return true;
      }
      case type_id_v<circle>: {
        auto tmp = circle{};
        continuation(tmp);
        return true;
      }
    }
  }
};

template <>
struct inspector_access<shape_ptr> : variant_inspector_access<shape_ptr> {
  // nop
};

} // namespace caf

shape_ptr serialization_roundtrip(const shape_ptr& in) {
  caf::byte_buffer buf;
  caf::binary_serializer sink{nullptr, buf};
  if (!sink.apply(in)) {
    std::cerr << "failed to serialize shape!\n";
    return nullptr;
  }
  shape_ptr out;
  caf::binary_deserializer source{nullptr, buf};
  if (!source.apply(out)) {
    std::cerr << "failed to deserialize shape!\n";
    return nullptr;
  }
  return out;
}

void caf_main(caf::actor_system&) {
  std::vector<shape_ptr> shapes;
  shapes.emplace_back(nullptr);
  shapes.emplace_back(rectangle::make({10, 10}, {20, 20}));
  shapes.emplace_back(circle::make({15, 15}, 5));
  std::cout << "shapes:\n";
  for (auto& ptr : shapes) {
    std::cout << "  shape: " << caf::deep_to_string(ptr) << '\n';
    auto copy = serialization_roundtrip(ptr);
    assert(!ptr || ptr.get() != copy.get());
    std::cout << "   copy: " << caf::deep_to_string(copy) << '\n';
  }
}

CAF_MAIN(caf::id_block::custom_types_4)
