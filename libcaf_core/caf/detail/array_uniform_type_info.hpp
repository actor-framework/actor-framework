#ifndef CAF_ARRAY_UNIFORM_TYPE_INFO_HPP
#define CAF_ARRAY_UNIFORM_TYPE_INFO_HPP
#include "caf/abstract_uniform_type_info.hpp"

namespace caf {

namespace detail {

template <typename T, size_t N>
class array_uniform_type_info : public abstract_uniform_type_info<T[N]> {
public:
  array_uniform_type_info(std::string name)
      : abstract_uniform_type_info<T[N]>(std::move(name)) {
    // nop
  }

protected:
  void serialize(const void* ptr, serializer* sink) const {
    auto array_ptr = reinterpret_cast<const T*>(ptr);
    for (size_t i = 0; i < N; i++) {
        sink->write_value(array_ptr[i]);
    }
  }

  void deserialize(void* ptr, deserializer* source) const {
    auto array_ptr = reinterpret_cast<T*>(ptr);
    for (size_t i = 0; i < N; i++) {
        array_ptr[i] = source->read<T>();
    }
  }
};

} //namespace detail

} //namespace caf

#endif
