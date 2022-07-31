#include "caf/detail/json.hpp"

using namespace caf;

thread_local detail::monotonic_buffer_resource buf;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  buf.reclaim();
  std::string_view json_text{reinterpret_cast<const char*>(data), size};
  string_parser_state ps{json_text.begin(), json_text.end()};
  detail::json::parse(ps, &buf);
  return 0;
}
