// HTTP client example that uploads one or more files as
// multipart/form-data using an HTTP POST request.
//
// Usage:
//   multipart-writer <url> <field-name-1> <file-path-1>
//                    [<field-name-2> <file-path-2> ...]
//
// For each <field-name>/<file-path> pair, this example creates one multipart
// part with Content-Disposition and a basic Content-Type inferred from the file
// extension (.jpg/.jpeg, .png, .pdf, otherwise text/plain).
//
// For HTTPS targets, pass --tls.ca-file=<path> to verify the server
// certificate against a custom CA bundle.

#include "caf/net/http/multipart_writer.hpp"
#include "caf/net/http/with.hpp"
#include "caf/net/middleman.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte_span.hpp"
#include "caf/caf_main.hpp"
#include "caf/deep_to_string.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace std::literals;

namespace http = caf::net::http;
namespace ssl = caf::net::ssl;
using namespace caf;

// -- configuration ------------------------------------------------------------

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "tls"} //
      .add<std::string>("ca-file", "CA file for trusted servers");
  }
};

namespace {

bool is_sanitized(std::string_view value) {
  return value.find_first_of("\r\n\"\\") == std::string_view::npos;
}

std::string_view content_type_by_extension(std::string_view file_extension) {
  if (file_extension == ".jpg" || file_extension == ".jpeg")
    return "image/jpeg";
  if (file_extension == ".png")
    return "image/png";
  if (file_extension == ".pdf")
    return "application/pdf";
  return "text/plain";
}

std::optional<std::vector<std::byte>>
read_file(const std::filesystem::path& file_path) {
  std::unique_ptr<FILE, int (*)(FILE*)> file{
    fopen(file_path.string().c_str(), "rb"), fclose};
  if (!file)
    return std::nullopt;
  if (fseek(file.get(), 0, SEEK_END) != 0)
    return std::nullopt;
  auto size = ftell(file.get());
  if (size < 0)
    return std::nullopt;
  if (fseek(file.get(), 0, SEEK_SET) != 0)
    return std::nullopt;
  std::vector<std::byte> result(static_cast<size_t>(size));
  if (!result.empty()) {
    auto bytes_read = fread(result.data(), 1, result.size(), file.get());
    if (bytes_read != result.size() || ferror(file.get()) != 0)
      return std::nullopt;
  }
  return result;
}

} // namespace

// -- main ---------------------------------------------------------------------

int caf_main(caf::actor_system& sys, const config& cfg) {
  auto remainder = cfg.remainder();
  if (remainder.size() < 1) {
    sys.println("*** expected mandatory positional argument: URL");
    return EXIT_FAILURE;
  }
  uri resource;
  if (auto err = parse(remainder[0], resource); err.valid()) {
    sys.println("*** failed to parse URI: {}", err);
    return EXIT_FAILURE;
  }
  if (remainder.size() % 2 != 1) {
    sys.println("*** expected field name alongside file path");
    return EXIT_FAILURE;
  }
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
  http::multipart_writer writer;
  for (size_t i = 1; i < remainder.size(); i += 2) {
    auto field_name = std::string{remainder[i]};
    if (!is_sanitized(field_name)) {
      sys.println("*** invalid field name: contains reserved characters");
      return EXIT_FAILURE;
    }
    auto file_path = std::filesystem::path{remainder[i + 1]};
    auto filename = file_path.filename().string();
    if (!is_sanitized(filename)) {
      sys.println("*** invalid filename: contains reserved characters");
      return EXIT_FAILURE;
    }
    auto file_data = read_file(file_path);
    if (!file_data) {
      sys.println("*** failed to read file: {}", file_path.string());
      return EXIT_FAILURE;
    }
    writer.append(*file_data,
                  [field_name = std::move(field_name),
                   filename = std::move(filename),
                   file_content_type = content_type_by_extension(
                     file_path.extension().string())](auto& headers) {
                    auto name = "form-data; name=\"" + field_name
                                + "\"; filename=\"" + filename + "\"";
                    headers.add("Content-Disposition", name);
                    headers.add("Content-Type", file_content_type);
                  });
  }
  auto result = http::with(sys)
                  // Lazy load TLS when connecting to HTTPS endpoints.
                  .context_factory([ca_file, resource]() {
                    return ssl::emplace_client(ssl::tls::v1_2)()
                      .and_then(ssl::load_verify_file_if(ca_file))
                      .and_then(ssl::use_sni_hostname(resource));
                  })
                  .connect(resource)
                  .retry_delay(1s)
                  .max_retry_count(5)
                  .connection_timeout(250ms)
                  .add_header_field("Content-Type", writer.make_content_type())
                  .request(http::method::post, writer.finalize());
  if (!result) {
    sys.println("*** Failed to initiate connection: {}", result.error());
    return EXIT_FAILURE;
  }
  auto response = result->first.get(10s);
  if (!response) {
    sys.println("*** HTTP request failed: {}", response.error());
    return EXIT_FAILURE;
  }
  auto& r = *response;
  sys.println("Server responded with HTTP {}: {}",
              static_cast<uint16_t>(r.code()), phrase(r.code()));
  sys.println("Header fields:");
  for (const auto& [key, value] : r.header_fields())
    sys.println("- {}: {}", key, value);
  if (r.body().empty())
    return EXIT_SUCCESS;
  if (is_valid_utf8(r.body())) {
    sys.println("Payload: {}", to_string_view(r.body()));
  } else {
    auto split_at = [](const_byte_span bytes, size_t at) {
      if (bytes.size() > at)
        return std::pair{bytes.subspan(0, at), bytes.subspan(at)};
      return std::pair{bytes, const_byte_span{}};
    };
    // Print 8 bytes per row in hex.
    sys.println("Payload:");
    auto bytes = r.body();
    const_byte_span row;
    while (!bytes.empty()) {
      std::tie(row, bytes) = split_at(bytes, 8);
      sys.println("{}", to_hex_str(row));
    }
  }
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
