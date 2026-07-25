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
#include "caf/string_algorithms.hpp"

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

// Custom config for adding a command line option for the CA file.
struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "tls"} //
      .add<std::string>("ca-file", "CA file for trusted servers");
  }
};

// Checks whether a string contains any reserved characters that would make it
// unsafe to use in a multipart header field.
bool is_sanitized(std::string_view value) {
  return value.find_first_of("\r\n\"\\") == std::string_view::npos;
}

// Returns a MIME content type based on the file extension.
std::string_view content_type_by_extension(std::string_view extension) {
  if (icase_equal(extension, ".jpg") || icase_equal(extension, ".jpeg")) {
    return "image/jpeg";
  }
  if (icase_equal(extension, ".png")) {
    return "image/png";
  }
  if (icase_equal(extension, ".pdf")) {
    return "application/pdf";
  }
  return "text/plain";
}

// Custom deleter for std::unique_ptr to automatically close FILE* handles.
struct fcloser {
  void operator()(FILE* file) const noexcept {
    if (file != nullptr) {
      fclose(file);
    }
  }
};

// Reads the entire contents of a file into a vector of bytes.
std::optional<std::vector<std::byte>>
read_file(const std::filesystem::path& file_path) {
  using file_ptr = std::unique_ptr<FILE, fcloser>;
  auto file = file_ptr{fopen(file_path.string().c_str(), "rb")};
  if (!file)
    return std::nullopt;
  if (fseek(file.get(), 0, SEEK_END) != 0)
    return std::nullopt;
  auto size = ftell(file.get());
  if (size <= 0)
    return std::nullopt;
  if (fseek(file.get(), 0, SEEK_SET) != 0)
    return std::nullopt;
  std::vector<std::byte> result;
  result.resize(static_cast<size_t>(size));
  auto bytes_read = fread(result.data(), 1, result.size(), file.get());
  if (bytes_read != result.size() || ferror(file.get()) != 0)
    return std::nullopt;
  return result;
}

int caf_main(caf::actor_system& sys, const config& cfg) {
  // Parse command line arguments.
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
  // Feed all fields and files into a multipart writer.
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
    auto name = "form-data; name=\"" + field_name + "\"; filename=\"" + filename
                + "\"";
    auto extension = file_path.extension().string();
    auto content_type = content_type_by_extension(extension);
    writer.append(*file_data, [&name, content_type](auto& headers) {
      headers.add("Content-Disposition", name);
      headers.add("Content-Type", content_type);
    });
  }
  // Send a POST request with the payload from the multipart writer.
  auto ca_file = caf::get_as<std::string>(cfg, "tls.ca-file");
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
  // Wait for the response and print it to stdout.
  auto maybe_response = result->first.get(10s);
  if (!maybe_response) {
    sys.println("*** HTTP request failed: {}", maybe_response.error());
    return EXIT_FAILURE;
  }
  auto& response = *maybe_response;
  sys.println("Server responded with HTTP code {}",
              static_cast<uint16_t>(response.code()));
  sys.println("Header fields:");
  for (const auto& [key, value] : response.header_fields())
    sys.println("- {}: {}", key, value);
  if (auto body = response.body(); !body.empty()) {
    if (is_valid_utf8(body)) {
      sys.println("Payload (UTF-8): {}", to_string_view(body));
    } else {
      sys.println("Payload (binary): {}", to_hex_str(body));
    }
  }
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
