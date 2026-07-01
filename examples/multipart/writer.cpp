// Simple HTTP client that sends multipart/form-data.

#include "caf/net/http/multipart_writer.hpp"
#include "caf/net/http/with.hpp"
#include "caf/net/middleman.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/byte_span.hpp"
#include "caf/caf_main.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/event_based_actor.hpp"

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

std::string make_content_type(std::string_view boundary) {
  return "multipart/form-data; boundary=" + std::string{boundary};
}

std::optional<std::string> sanitize_quoted_value(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (auto ch : value) {
    if (ch == '\r' || ch == '\n')
      return std::nullopt;
    if (ch == '"' || ch == '\\')
      result.push_back('\\');
    result.push_back(ch);
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
    auto field_name = sanitize_quoted_value(remainder[i]);
    if (!field_name) {
      sys.println("*** invalid field name: contains CR/LF");
      return EXIT_FAILURE;
    }
    auto file_path = std::filesystem::path{remainder[i + 1]};
    auto filename = sanitize_quoted_value(file_path.filename().string());
    if (!filename) {
      sys.println("*** invalid filename: contains CR/LF");
      return EXIT_FAILURE;
    }
    std::unique_ptr<FILE, int (*)(FILE*)> file{
      fopen(file_path.string().c_str(), "rb"), fclose};
    if (!file) {
      sys.println("*** failed to read file: {}", file_path.string());
      return EXIT_FAILURE;
    }
    // Generate the content-type header based on the file extension.
    auto file_extension = file_path.extension().string();
    auto file_content_type = "text/plain";
    if (file_extension == ".jpg" || file_extension == ".jpeg")
      file_content_type = "image/jpeg";
    else if (file_extension == ".png")
      file_content_type = "image/png";
    else if (file_extension == ".pdf")
      file_content_type = "application/pdf";
    // Read complete file content and append it as a single multipart part.
    std::vector<std::byte> file_data;
    char buffer[4096];
    auto n = fread(buffer, 1, sizeof(buffer), file.get());
    while (n > 0) {
      auto first = reinterpret_cast<const std::byte*>(buffer);
      file_data.insert(file_data.end(), first, first + n);
      n = fread(buffer, 1, sizeof(buffer), file.get());
    }
    if (ferror(file.get()) != 0) {
      sys.println("*** failed while reading file: {}", file_path.string());
      return EXIT_FAILURE;
    }
    writer.append(file_data, [field_name = std::move(*field_name),
                              filename = std::move(*filename),
                              file_content_type](auto& headers) {
      auto name = std::string{"form-data; name=\""} + field_name
                  + "\"; filename=\"" + filename + "\"";
      headers.add("Content-Disposition", name);
      headers.add("Content-Type", file_content_type);
    });
  }
  auto payload = writer.finalize();
  auto content_type = make_content_type(writer.boundary());
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
                  .add_header_field("User-Agent", "CAF-Client")
                  .add_header_field("Content-Type", content_type)
                  .request(http::method::post, payload);
  if (!result) {
    sys.println("*** Failed to initiate connection: {}", result.error());
    return EXIT_FAILURE;
  }
  sys.spawn([res = result->first](event_based_actor* self) {
    res.bind_to(self).then(
      [self](const http::response& r) {
        self->println("Server responded with HTTP {}: {}",
                      static_cast<uint16_t>(r.code()), phrase(r.code()));
        self->println("Header fields:");
        for (const auto& [key, value] : r.header_fields())
          self->println("- {}: {}", key, value);
        if (r.body().empty())
          return;
        if (is_valid_utf8(r.body())) {
          self->println("Payload: {}", to_string_view(r.body()));
        } else {
          auto split_at = [](const_byte_span bytes, size_t at) {
            if (bytes.size() > at)
              return std::pair{bytes.subspan(0, at), bytes.subspan(at)};
            return std::pair{bytes, const_byte_span{}};
          };
          // Print 8 bytes per row in hex.
          self->println("Payload:");
          auto bytes = r.body();
          const_byte_span row;
          while (!bytes.empty()) {
            std::tie(row, bytes) = split_at(bytes, 8);
            self->println("{}", to_hex_str(row));
          }
        }
      },
      [self](const error& err) {
        self->println("*** HTTP request failed: {}", err);
      });
  });
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
