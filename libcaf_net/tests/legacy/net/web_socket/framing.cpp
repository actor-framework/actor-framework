// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE net.web_socket.framing

#include "caf/net/web_socket/framing.hpp"

#include "net-test.hpp"

using namespace caf;
using namespace std::literals;

namespace {

struct fixture {
  mock_web_socket_app* app = nullptr;
  net::web_socket::framing* uut = nullptr;
  std::unique_ptr<mock_stream_transport> transport;

  fixture() {
    reset();
  }

  void reset() {
    auto app_layer = mock_web_socket_app::make();
    app = app_layer.get();
    auto uut_layer
      = net::web_socket::framing::make_server(std::move(app_layer));
    uut = uut_layer.get();
    transport = mock_stream_transport::make(std::move(uut_layer));
    CHECK_EQ(transport->start(nullptr), error{});
    transport->configure_read(caf::net::receive_policy::up_to(2048u));
  }
};

byte_buffer make_test_data(size_t requested_size) {
  return byte_buffer{requested_size, std::byte{0xFF}};
}

auto bytes(std::initializer_list<uint8_t> xs) {
  byte_buffer result;
  for (auto x : xs)
    result.emplace_back(static_cast<std::byte>(x));
  return result;
}

int fetch_status(const_byte_span payload) {
  return (std::to_integer<int>(payload[2]) << 8)
         + std::to_integer<int>(payload[3]);
}

byte_buffer make_closing_payload(uint16_t code_val, std::string_view msg) {
  byte_buffer payload;
  payload.push_back(static_cast<std::byte>((code_val & 0xFF00) >> 8));
  payload.push_back(static_cast<std::byte>(code_val & 0x00FF));
  auto* first = reinterpret_cast<const std::byte*>(msg.data());
  payload.insert(payload.end(), first, first + msg.size());
  return payload;
}

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("the client sends a ping and receives a pong response") {
  GIVEN("a valid WebSocket connection") {
    std::vector<std::byte> ping_frame;
    std::vector<std::byte> pong_frame;
    WHEN("the client sends a ping with no data") {
      auto data = make_test_data(0);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      CHECK_EQ(transport->handle_input(),
               static_cast<ptrdiff_t>(ping_frame.size()));
      THEN("the server responds with an empty pong") {
        detail::rfc6455::assemble_frame(detail::rfc6455::pong, 0x0, data,
                                        pong_frame);
        CHECK_EQ(transport->output_buffer(), pong_frame);
      }
      AND("the client did not abort") {
        CHECK(!app->has_aborted());
      }
    }
    transport->output_buffer().clear();
    WHEN("the client sends a ping with some data") {
      auto data = make_test_data(40);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      CHECK_EQ(transport->handle_input(),
               static_cast<ptrdiff_t>(ping_frame.size()));
      THEN("the server echoes the data back to the client") {
        detail::rfc6455::assemble_frame(detail::rfc6455::pong, 0x0, data,
                                        pong_frame);
        CHECK_EQ(transport->output_buffer(), pong_frame);
      }
      AND("the client did not abort") {
        CHECK(!app->has_aborted());
      }
    }
    transport->output_buffer().clear();
    WHEN("the client sends a ping with the maximum allowed 125 bytes") {
      auto data = make_test_data(125);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      CHECK_EQ(transport->handle_input(),
               static_cast<ptrdiff_t>(ping_frame.size()));
      THEN("the server echoes the data back to the client") {
        detail::rfc6455::assemble_frame(detail::rfc6455::pong, 0x0, data,
                                        pong_frame);
        CHECK_EQ(transport->output_buffer(), pong_frame);
      }
      AND("the client did not abort") {
        CHECK(!app->has_aborted());
      }
    }
  }
}

TEST_CASE("calling shutdown with protocol_error sets status in close header") {
  uut->shutdown(make_error(sec::protocol_error));
  detail::rfc6455::header hdr;
  detail::rfc6455::decode_header(transport->output_buffer(), hdr);
  CHECK_EQ(hdr.opcode, detail::rfc6455::connection_close);
  CHECK(hdr.payload_len >= 2);
  CHECK_EQ(fetch_status(transport->output_buffer()),
           static_cast<int>(net::web_socket::status::protocol_error));
  CHECK(!app->has_aborted());
}

SCENARIO("the client sends an invalid ping that closes the connection") {
  GIVEN("a valid WebSocket connection") {
    std::vector<std::byte> ping_frame;
    WHEN("the client sends a ping with more data than allowed") {
      auto data = make_test_data(126);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame);
      transport->push(ping_frame);
      THEN("the server aborts the application") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK(app->has_aborted());
        CHECK_EQ(app->abort_reason, sec::protocol_error);
        MESSAGE("Aborted with: " << app->abort_reason);
      }
      AND("the server closes the connection with a protocol error") {
        detail::rfc6455::header hdr;
        detail::rfc6455::decode_header(transport->output_buffer(), hdr);
        MESSAGE("Buffer: " << transport->output_buffer());
        CHECK_EQ(hdr.opcode, detail::rfc6455::connection_close);
        CHECK(hdr.payload_len >= 2);
        CHECK_EQ(fetch_status(transport->output_buffer()),
                 static_cast<int>(net::web_socket::status::protocol_error));
      }
    }
  }
}

SCENARIO("the client closes the connection with a closing handshake") {
  GIVEN("a valid WebSocket connection") {
    WHEN("the client sends a closing handshake") {
      std::vector<std::byte> handshake;
      detail::rfc6455::assemble_frame(detail::rfc6455::connection_close, 0x0,
                                      make_test_data(0), handshake);
      transport->push(handshake);
    }
    THEN("the server closes the connection after sending a close frame") {
      transport->handle_input();
      detail::rfc6455::header hdr;
      auto hdr_length
        = detail::rfc6455::decode_header(transport->output_buffer(), hdr);
      CHECK(app->has_aborted());
      CHECK_EQ(app->abort_reason, sec::connection_closed);
      CHECK_EQ(hdr_length, 2);
      CHECK_EQ(hdr.opcode, detail::rfc6455::connection_close);
      CHECK(hdr.fin);
      CHECK(hdr.payload_len >= 2);
      CHECK_EQ(fetch_status(transport->output_buffer()),
               static_cast<int>(net::web_socket::status::normal_close));
    }
    WHEN("the client sends an invalid closing handshake") {
      reset();
      std::vector<std::byte> handshake;
      // invalid status code
      auto payload = make_closing_payload(1016, ""sv);
      detail::rfc6455::assemble_frame(detail::rfc6455::connection_close, 0x0,
                                      payload, handshake);
      transport->push(handshake);
    }
    THEN("the server closes the connection with protocol error") {
      CHECK_EQ(transport->handle_input(), 0l);
      detail::rfc6455::header hdr;
      detail::rfc6455::decode_header(transport->output_buffer(), hdr);
      CHECK_EQ(app->abort_reason, sec::protocol_error);
      CHECK_EQ(hdr.opcode, detail::rfc6455::connection_close);
      CHECK(hdr.fin);
      CHECK_EQ(fetch_status(transport->output_buffer()),
               static_cast<int>(net::web_socket::status::protocol_error));
    }
  }
}

SCENARIO("ping messages may not be fragmented") {
  GIVEN("a valid WebSocket connection") {
    std::vector<std::byte> ping_frame;
    WHEN("the client sends the first frame of a fragmented ping message") {
      auto data = make_test_data(10);
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data,
                                      ping_frame, 0);
      transport->push(ping_frame);
      THEN("the server aborts the application") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK(app->has_aborted());
        CHECK_EQ(app->abort_reason, sec::protocol_error);
        MESSAGE("Aborted with: " << app->abort_reason);
      }
      AND("the server closes the connection with a protocol error") {
        detail::rfc6455::header hdr;
        detail::rfc6455::decode_header(transport->output_buffer(), hdr);
        MESSAGE("Buffer: " << transport->output_buffer());
        CHECK_EQ(hdr.opcode, detail::rfc6455::connection_close);
        CHECK(hdr.payload_len >= 2);
        CHECK_EQ(fetch_status(transport->output_buffer()),
                 static_cast<int>(net::web_socket::status::protocol_error));
      }
    }
  }
}

SCENARIO("ping messages may arrive between message fragments") {
  GIVEN("a valid WebSocket connection") {
    WHEN("the frames arrive all at once") {
      std::vector<std::byte> input;
      auto fragment1 = "Hello"sv;
      auto fragment2 = ", world!"sv;
      auto data = as_bytes(make_span(fragment1));
      detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0x0, data,
                                      input, 0);
      transport->push(input);
      input.clear();
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data, input);
      transport->push(input);
      input.clear();
      data = as_bytes(make_span(fragment2));
      detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame, 0x0,
                                      data, input);
      transport->push(input);
      transport->handle_input();
      THEN("the server responds with a pong") {
        detail::rfc6455::header hdr;
        auto hdr_len
          = detail::rfc6455::decode_header(transport->output_buffer(), hdr);
        MESSAGE("Payload: " << transport->output_buffer());
        CHECK_EQ(hdr_len, 2u);
        CHECK(hdr.fin);
        CHECK_EQ(hdr.opcode, detail::rfc6455::pong);
        CHECK_EQ(hdr.payload_len, 5u);
        CHECK_EQ(hdr.mask_key, 0u);
      }
      AND("the server receives the full text message") {
        CHECK_EQ(app->text_input, "Hello, world!"sv);
      }
      AND("the app is still running") {
        CHECK(!app->has_aborted());
      }
    }
    WHEN("the frames arrive separately") {
      reset();
      auto fragment1 = "Hello"sv;
      auto fragment2 = ", world!"sv;
      std::vector<std::byte> input;
      auto data = as_bytes(make_span(fragment1));
      detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0x0, data,
                                      input, 0);
      transport->push(input);
      transport->handle_input();
      THEN("the server receives nothing") {
        CHECK(app->text_input.empty());
        CHECK(app->binary_input.empty());
      }
      input.clear();
      detail::rfc6455::assemble_frame(detail::rfc6455::ping, 0x0, data, input);
      transport->push(input);
      transport->handle_input();
      THEN("the server responds with a pong") {
        detail::rfc6455::header hdr;
        auto hdr_len
          = detail::rfc6455::decode_header(transport->output_buffer(), hdr);
        CHECK_EQ(hdr_len, 2u);
        CHECK(hdr.fin);
        CHECK_EQ(hdr.opcode, detail::rfc6455::pong);
        CHECK_EQ(hdr.payload_len, 5u);
        CHECK_EQ(hdr.mask_key, 0u);
      }
      input.clear();
      data = as_bytes(make_span(fragment2));
      detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame, 0x0,
                                      data, input);
      transport->push(input);
      transport->handle_input();
      THEN("the server receives the full text message") {
        CHECK_EQ(app->text_input, "Hello, world!"sv);
      }
      AND("the client did not abort") {
        CHECK(!app->has_aborted());
      }
    }
  }
}

SCENARIO("the application shuts down on invalid UTF-8 message") {
  GIVEN("a text message with an invalid UTF-8 code point") {
    auto data = bytes({
      0xce, 0xba, 0xe1, 0xbd, 0xb9, 0xcf, // valid
      0x83, 0xce, 0xbc, 0xce, 0xb5,       // valid
      0xf4, 0x90, 0x80, 0x80,             // invalid code point
      0x65, 0x64, 0x69, 0x74, 0x65, 0x64  // valid
    });
    auto data_span = const_byte_span(data);
    WHEN("the client sends the whole message as a single frame") {
      reset();
      byte_buffer frame;
      detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0x0,
                                      data_span, frame);
      transport->push(frame);
      THEN("the server aborts the application") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::malformed_message);
        CHECK_EQ(fetch_status(transport->output_buffer()),
                 static_cast<int>(net::web_socket::status::inconsistent_data));
        MESSAGE("Aborted with: " << app->abort_reason);
      }
    }
    WHEN("the client sends the first part of the message") {
      reset();
      byte_buffer frame;
      detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0x0,
                                      data_span.subspan(0, 11), frame, 0);
      transport->push(frame);
      THEN("the connection did not abort") {
        CHECK_EQ(transport->handle_input(),
                 static_cast<ptrdiff_t>(frame.size()));
        CHECK(!app->has_aborted());
      }
    }
    AND_WHEN("the client sends the second frame containing invalid data") {
      byte_buffer frame;
      detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame, 0x0,
                                      data_span.subspan(11), frame);
      transport->push(frame);
      THEN("the server aborts the application") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::malformed_message);
        CHECK_EQ(fetch_status(transport->output_buffer()),
                 static_cast<int>(net::web_socket::status::inconsistent_data));
        MESSAGE("Aborted with: " << app->abort_reason);
      }
    }
    WHEN("the client sends the invalid byte on a frame boundary") {
      reset();
      byte_buffer frame;
      detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0x0,
                                      data_span.subspan(0, 12), frame, 0);
      transport->push(frame);
      CHECK_EQ(transport->handle_input(), static_cast<ptrdiff_t>(frame.size()));
      CHECK(!app->has_aborted());
      frame.clear();
      detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame, 0x0,
                                      data_span.subspan(12, 1), frame, 0);
      transport->push(frame);
      THEN("the server aborts the application") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::malformed_message);
        CHECK_EQ(fetch_status(transport->output_buffer()),
                 static_cast<int>(net::web_socket::status::inconsistent_data));
        MESSAGE("Aborted with: " << app->abort_reason);
      }
    }
    WHEN("the client sends an invalid text frame byte by byte") {
      reset();
      byte_buffer frame;
      detail::rfc6455::mask_data(0xDEADC0DE, data);
      detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0xDEADC0DE,
                                      data_span, frame, 0);
      for (auto i = 0; i < 18; i++) {
        transport->push(make_span(frame).subspan(i, 1));
        CHECK_EQ(transport->handle_input(), 0);
        CHECK(!app->has_aborted());
      }
      THEN("the server aborts when receiving the invalid byte") {
        transport->push(make_span(frame).subspan(18, 1));
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::malformed_message);
        CHECK_EQ(fetch_status(transport->output_buffer()),
                 static_cast<int>(net::web_socket::status::inconsistent_data));
        MESSAGE("Aborted with: " << app->abort_reason);
      }
      WHEN("the client sends the first frame of a text messagee") {
        reset();
        byte_buffer frame;
        detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0xDEADC0DE,
                                        data_span.subspan(0, 8), frame, 0);
        transport->push(frame);
        CHECK_EQ(transport->handle_input(),
                 static_cast<ptrdiff_t>(frame.size()));
        CHECK(!app->has_aborted());
      }
      AND_WHEN("sending the invalid continuation frame byte by byte") {
        byte_buffer frame;
        detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame,
                                        0xDEADC0DE, data_span.subspan(8),
                                        frame);
        for (auto i = 0; i < 10; i++) {
          transport->push(make_span(frame).subspan(i, 1));
          CHECK_EQ(transport->handle_input(), 0);
          CHECK(!app->has_aborted());
        }
        THEN("the server aborts the application on the invalid byte") {
          transport->push(make_span(frame).subspan(10, 1));
          CHECK_EQ(transport->handle_input(), 0);
          CHECK_EQ(app->abort_reason, sec::malformed_message);
          CHECK_EQ(fetch_status(transport->output_buffer()),
                   static_cast<int>(
                     net::web_socket::status::inconsistent_data));
          MESSAGE("Aborted with: " << app->abort_reason);
        }
      }
    }
  }
}

END_FIXTURE_SCOPE()

namespace {

// The following setup is a mock websocket application that rejects everything

struct rejecting_mock_web_socket_app : public mock_web_socket_app {
  rejecting_mock_web_socket_app() : mock_web_socket_app(false) {
  }
  ptrdiff_t consume_text(std::string_view) override {
    abort(make_error(sec::logic_error));
    return -1;
  }
  ptrdiff_t consume_binary(caf::byte_span) override {
    abort(make_error(sec::logic_error));
    return -1;
  }
};

struct rejecting_fixture {
  rejecting_mock_web_socket_app* app = nullptr;
  net::web_socket::framing* uut = nullptr;
  std::unique_ptr<mock_stream_transport> transport;

  void reset() {
    auto app_layer = std::make_unique<rejecting_mock_web_socket_app>();
    app = app_layer.get();
    auto uut_layer
      = net::web_socket::framing::make_server(std::move(app_layer));
    uut = uut_layer.get();
    transport = mock_stream_transport::make(std::move(uut_layer));
    CHECK_EQ(transport->start(nullptr), error{});
    transport->configure_read(caf::net::receive_policy::up_to(2048u));
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(rejecting_fixture)

SCENARIO("apps can return errors to shut down the framing layer") {
  GIVEN("an app that returns -1 for any frame it receives") {
    WHEN("receiving a binary message") {
      THEN("the framing layer closes the connection") {
        reset();
        byte_buffer input;
        const auto data = make_test_data(4);
        detail::rfc6455::assemble_frame(0x0, data, input);
        transport->push(input);
        CHECK_EQ(transport->handle_input(), 0);
        CHECK(app->has_aborted());
      }
    }
    WHEN("receiving a fragmented binary message") {
      THEN("the framing layer closes the connection") {
        reset();
        byte_buffer frame1;
        byte_buffer frame2;
        const auto data = make_test_data(4);
        detail::rfc6455::assemble_frame(detail::rfc6455::binary_frame, 0x0,
                                        data, frame1, 0);
        transport->push(frame1);
        detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame,
                                        0x0, data, frame2);
        transport->push(frame2);
        // We only handle one frame, the second one results in an abort
        CHECK_EQ(transport->handle_input(),
                 static_cast<ptrdiff_t>(frame1.size()));
        CHECK(app->has_aborted());
      }
    }
    WHEN("receiving a text message") {
      THEN("the framing layer closes the connection") {
        reset();
        byte_buffer input;
        const auto msg = "Hello, world!"sv;
        detail::rfc6455::assemble_frame(0x0, make_span(msg), input);
        transport->push(input);
        CHECK_EQ(transport->handle_input(), 0);
        CHECK(app->has_aborted());
      }
    }
    WHEN("receiving a fragmented text message") {
      THEN("the framing layer closes the connection") {
        reset();
        byte_buffer frame1;
        byte_buffer frame2;
        const auto msg = "Hello, world!"sv;
        detail::rfc6455::assemble_frame(detail::rfc6455::text_frame, 0x0,
                                        as_bytes(make_span(msg)), frame1, 0);
        transport->push(frame1);
        detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame,
                                        0x0, as_bytes(make_span(msg)), frame2);
        transport->push(frame2);
        CHECK_EQ(transport->handle_input(),
                 static_cast<ptrdiff_t>(frame1.size()));
        CHECK(app->has_aborted());
      }
    }
  }
}

SCENARIO("the application receives a pong") {
  GIVEN("a valid WebSocket connection") {
    WHEN("the client sends a pong") {
      reset();
      byte_buffer input;
      const auto data = make_test_data(10);
      detail::rfc6455::assemble_frame(detail::rfc6455::pong, 0x0, data, input);
      transport->push(input);
      THEN("the application handles the frame without actual input") {
        CHECK_EQ(transport->handle_input(),
                 static_cast<ptrdiff_t>(input.size()));
        CHECK(app->text_input.empty());
        CHECK(app->binary_input.empty());
      }
      AND("the application is still working") {
        CHECK(!app->has_aborted());
      }
    }
  }
}

SCENARIO("apps reject frames whose payload exceeds maximum allowed size") {
  GIVEN("a WebSocket app accepting payloads up to INT32_MAX") {
    WHEN("receiving a single large frame") {
      reset();
      // Faking a large frame without the actual payload
      auto frame
        = bytes({0x82, // FIN + binary frame opcode
                 0x7F, // NO MASK + 127 -> uint64 size
                 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, // 2 ^ 31
                 0xFF, 0xFF, 0xFF, 0xFF}); // first 4 bytes
      transport->push(frame);
      THEN("the app closes the connection with a protocol error") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::protocol_error);
        MESSAGE("Aborted with: " << app->abort_reason);
      }
    }
    WHEN("receiving fragmented frames whose combined payload is too large") {
      reset();
      byte_buffer frame;
      auto data = make_test_data(256);
      detail::rfc6455::assemble_frame(detail::rfc6455::binary_frame, 0x0, data,
                                      frame, 0);
      transport->push(frame);
      CHECK_EQ(transport->handle_input(), static_cast<ptrdiff_t>(frame.size()));
      frame.clear();
      frame = bytes({0x82, // FIN + continuation frame opcode
                     0x7F, // NO MASK + 127 -> uint64 size
                     0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff,
                     0x00,                     // 2 ^ 31 - 256
                     0xFF, 0xFF, 0xFF, 0xFF}); // first 4 masked bytes
      transport->push(frame);
      THEN("the app closes the connection with a protocol error") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::protocol_error);
        MESSAGE("Aborted with: " << app->abort_reason);
      }
    }
  }
}

SCENARIO("the application shuts down on invalid frame fragments") {
  GIVEN("a client that sends invalid fragmented frames") {
    WHEN("the first fragment is a continuation frame with FIN flag") {
      reset();
      byte_buffer input;
      const auto data = make_test_data(10);
      detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame, 0x0,
                                      data, input);
      transport->push(input);
      THEN("the app closes the connection with a protocol error") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::protocol_error);
      }
    }
    WHEN("the first fragment is a continuation frame without FIN flag") {
      reset();
      byte_buffer input;
      const auto data = make_test_data(10);
      detail::rfc6455::assemble_frame(detail::rfc6455::continuation_frame, 0x0,
                                      data, input, 0);
      transport->push(input);
      THEN("the app closes the connection with a protocol error") {
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::protocol_error);
      }
    }
    WHEN("two starting fragments are received") {
      reset();
      byte_buffer input;
      const auto data = make_test_data(10);
      detail::rfc6455::assemble_frame(detail::rfc6455::binary_frame, 0x0, data,
                                      input, 0);
      THEN("the app accepts the first frame") {
        transport->push(input);
        CHECK_EQ(transport->handle_input(),
                 static_cast<ptrdiff_t>(input.size()));
      }
      AND("the app closes the connection after the second frame") {
        transport->push(input);
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::protocol_error);
      }
    }
    WHEN("the final frame is not a continuation frame") {
      reset();
      byte_buffer input;
      const auto data = make_test_data(10);
      detail::rfc6455::assemble_frame(detail::rfc6455::binary_frame, 0x0, data,
                                      input, 0);
      THEN("the app accepts the first frame") {
        transport->push(input);
        CHECK_EQ(transport->handle_input(),
                 static_cast<ptrdiff_t>(input.size()));
      }
      AND("the app closes the connection after the second frame") {
        input.clear();
        detail::rfc6455::assemble_frame(detail::rfc6455::binary_frame, 0x0,
                                        data, input);
        transport->push(input);
        CHECK_EQ(transport->handle_input(), 0);
        CHECK_EQ(app->abort_reason, sec::protocol_error);
      }
    }
  }
}

END_FIXTURE_SCOPE()

TEST_CASE("empty closing payload is valid") {
  auto error
    = net::web_socket::framing::validate_closing_payload(byte_buffer{});
  CHECK(!error);
}

TEST_CASE("decode valid closing codes") {
  auto valid_status_codes = std::array{1000, 1001, 1002, 1003, 1007, 1008, 1009,
                                       1010, 1011, 3000, 3999, 4000, 4999};
  for (auto status_code : valid_status_codes) {
    auto payload = make_closing_payload(status_code, ""sv);
    auto error = net::web_socket::framing::validate_closing_payload(payload);
    CHECK(!error);
  }
}

TEST_CASE("fail on invalid closing codes") {
  auto valid_status_codes = std::array{0,    999,  1004, 1005, 1006, 1016,
                                       1100, 2000, 2999, 5000, 65535};
  for (auto status_code : valid_status_codes) {
    auto payload = make_closing_payload(status_code, ""sv);
    auto result = net::web_socket::framing::validate_closing_payload(payload);
    CHECK_EQ(result, sec::protocol_error);
  }
}

TEST_CASE("fail on invalid utf8 closing message") {
  auto payload = make_closing_payload(1000, "\xf4\x80"sv);
  auto result = net::web_socket::framing::validate_closing_payload(payload);
  CHECK_EQ(result, sec::protocol_error);
}

TEST_CASE("fail on single byte payload") {
  auto payload = byte_buffer{std::byte{0}};
  auto result = net::web_socket::framing::validate_closing_payload(payload);
  CHECK_EQ(result, sec::protocol_error);
}
