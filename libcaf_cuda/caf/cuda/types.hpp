#pragma once
//file full of either forward declarations 
//or types used in caf cuda


#include <caf/intrusive_ptr.hpp>
#include <variant>
#include <vector>
#include <stdexcept>

#if defined(_MSC_VER)
  #define CAF_CUDA_EXPORT __declspec(dllexport)
#else
  #define CAF_CUDA_EXPORT __attribute__((visibility("default")))
#endif


//memory access flags, required for identifying which
//gpu buffers are input and output buffers
#define IN 0 
#define IN_OUT 1
#define OUT 2
#define NOT_IN_USE -1



namespace caf::cuda {

// Forward declarations and intrusive_ptr aliases

class device;
using device_ptr = caf::intrusive_ptr<device>;

class platform;
using platform_ptr = caf::intrusive_ptr<platform>;

class program;
using program_ptr = caf::intrusive_ptr<program>;

template <class T>
class mem_ref;

template <class T>
using mem_ptr = caf::intrusive_ptr<mem_ref<T>>;

// Forward declare manager, command, and actor_facade

class CAF_CUDA_EXPORT manager;

template <class Actor, class... Ts>
class command;

template <bool PassConfig, class... Ts>
class actor_facade;

} // namespace caf::cuda


// === buffer_variant and output_buffer outside namespace or inside as needed ===

using buffer_variant = std::variant<std::vector<char>, std::vector<int>, std::vector<float>, std::vector<double>>;

struct output_buffer {
  buffer_variant data;
};


// === Wrapper types for in/out/in_out with default ctor, union safely used ===

//represents a readonly buffer on the gpu
template <typename T>
class in_impl {
private:
  std::variant<T, std::vector<T>> data_;
  bool moved_from_ = false;

  void check_valid() const {
    if (moved_from_)
      throw std::runtime_error(std::string("Use-after-move detected in ") + typeid(*this).name());
  }

public:
  using value_type = T;

  // Constructors
  in_impl() : data_(T{}) {}
  explicit in_impl(const T& val) : data_(val) {}
  explicit in_impl(const std::vector<T>& buf) : data_(buf) {}

  // Copy constructor/assignment
  in_impl(const in_impl&) = default;
  in_impl& operator=(const in_impl&) = default;

  // Move constructor
  in_impl(in_impl&& other) noexcept
    : data_(std::move(other.data_)) {
    other.moved_from_ = true;
  }

  // Move assignment
  in_impl& operator=(in_impl&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      other.moved_from_ = true;
    }
    return *this;
  }

  bool is_scalar() const {
    check_valid();
    return std::holds_alternative<T>(data_);
  }

  const T& getscalar() const {
    check_valid();
    if (!is_scalar())
      throw std::runtime_error("in_impl does not hold scalar");
    return std::get<T>(data_);
  }

  const std::vector<T>& get_buffer() const {
    check_valid();
    if (is_scalar())
      throw std::runtime_error("in_impl does not hold buffer");
    return std::get<std::vector<T>>(data_);
  }

  const T* data() const {
    check_valid();
    return is_scalar() ? &std::get<T>(data_) : std::get<std::vector<T>>(data_).data();
  }

  std::size_t size() const {
    check_valid();
    return is_scalar() ? 1 : std::get<std::vector<T>>(data_).size();
  }
};


//represents a write only buffer on the gpu
template <typename T>
class out_impl {
private:
  std::variant<T, std::vector<T>> data_;
  bool moved_from_ = false;
  int size_ = 0; // Always store as int

  void check_valid() const {
    if (moved_from_)
      throw std::runtime_error(std::string("Use-after-move detected in ") + typeid(*this).name());
  }

public:
  using value_type = T;

  out_impl() : data_(T{}), size_(1) {}
  
  // Replaces old scalar constructor: just set size
  explicit out_impl(int size) 
    : data_(std::vector<T>(size)), size_(size) {}
  
  explicit out_impl(const std::vector<T>& buf) 
    : data_(buf), size_(static_cast<int>(buf.size())) {}

  out_impl(const out_impl&) = default;
  out_impl& operator=(const out_impl&) = default;

  out_impl(out_impl&& other) noexcept
    : data_(std::move(other.data_)), size_(other.size_) {
    other.moved_from_ = true;
  }

  out_impl& operator=(out_impl&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      size_ = other.size_;
      other.moved_from_ = true;
    }
    return *this;
  }

  bool is_scalar() const {
    check_valid();
    return std::holds_alternative<T>(data_);
  }

   const std::vector<T>& get_buffer() const {
    check_valid();
    if (is_scalar())
      throw std::runtime_error("out_impl does not hold buffer");
    return std::get<std::vector<T>>(data_);
  }

  const T* data() const {
    check_valid();
    return is_scalar() ? reinterpret_cast<const T*>(&size_)
                       : std::get<std::vector<T>>(data_).data();
  }

  size_t size() const {
    check_valid();
    return static_cast<size_t>(size_);
  }

};


//creates a read-write buffer on the gpu
template <typename T>
class in_out_impl {
private:
  std::variant<T, std::vector<T>> data_;
  bool moved_from_ = false;

  void check_valid() const {
    if (moved_from_)
      throw std::runtime_error(std::string("Use-after-move detected in ") + typeid(*this).name());
  }

public:
  using value_type = T;

  in_out_impl() : data_(T{}) {}
  explicit in_out_impl(const T& val) : data_(val) {}
  explicit in_out_impl(const std::vector<T>& buf) : data_(buf) {}

  in_out_impl(const in_out_impl&) = default;
  in_out_impl& operator=(const in_out_impl&) = default;

  in_out_impl(in_out_impl&& other) noexcept
    : data_(std::move(other.data_)) {
    other.moved_from_ = true;
  }

  in_out_impl& operator=(in_out_impl&& other) noexcept {
    if (this != &other) {
      data_ = std::move(other.data_);
      other.moved_from_ = true;
    }
    return *this;
  }

  bool is_scalar() const {
    check_valid();
    return std::holds_alternative<T>(data_);
  }

  const T& getscalar() const {
    check_valid();
    if (!is_scalar())
      throw std::runtime_error("in_out_impl does not hold scalar");
    return std::get<T>(data_);
  }

  const std::vector<T>& get_buffer() const {
    check_valid();
    if (is_scalar())
      throw std::runtime_error("in_out_impl does not hold buffer");
    return std::get<std::vector<T>>(data_);
  }

  const T* data() const {
    check_valid();
    return is_scalar() ? &std::get<T>(data_) : std::get<std::vector<T>>(data_).data();
  }

  std::size_t size() const {
    check_valid();
    return is_scalar() ? 1 : std::get<std::vector<T>>(data_).size();
  }
};

// === Aliases ===

//readonly buffer
template <typename T>
using in = in_impl<T>;

//writeonly buffer
template <typename T>
using out = out_impl<T>;


//readwrite buffer
template <typename T>
using in_out = in_out_impl<T>;

// Helper to get raw type inside wrapper
template <typename T>
struct raw_type {
  using type = T;
};

template <typename T>
struct raw_type<in<T>> {
  using type = T;
};

template <typename T>
struct raw_type<out<T>> {
  using type = T;
};

template <typename T>
struct raw_type<in_out<T>> {
  using type = T;
};

template <typename T>
struct raw_type<caf::cuda::mem_ptr<T>> {
  using type = T;
};

template <typename T>
using raw_t = typename raw_type<T>::type;


