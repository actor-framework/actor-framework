// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/format.hpp"

#include "caf/config.hpp"
#include "caf/detail/consumer.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/read_unsigned_integer.hpp"
#include "caf/detail/print.hpp"
#include "caf/parser_state.hpp"
#include "caf/pec.hpp"
#include "caf/raise_error.hpp"
#include "caf/span.hpp"

#include <type_traits>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

struct copy_state {
  using fn_t = void (*)(string_parser_state&, copy_state&);
  using maybe_uint = std::optional<size_t>;

  explicit copy_state(span<format_arg> arg_list);

  size_t next_arg_index = 0; // The next argument index for "auto"-mode.
  maybe_uint arg_index;      // The current argument index.
  maybe_uint width;          // The current width for formatting.
  bool is_number = false;    // Whether we are currently parsing a number.
  char fill = ' ';           // The fill character when setting an alignment.
  char align = 0;            // Alignment; '<': left, '>': right, '^': center.
  char type = 0;             // Selects an integer/float representation.
  span<format_arg> args;     // The list of arguments to use for formatting.
  fn_t fn;                   // The current state function.
  std::vector<char> buf;     // Stores the formatted string.
  std::vector<char> fstr;    // Assembles the format string for snprintf.

  // Restores the default values.
  void reset() {
    arg_index = std::nullopt;
    width = std::nullopt;
    fill = ' ';
    align = 0;
    type = 0;
    is_number = false;
    fstr.clear();
    fstr.push_back('%');
  }

  // Callback for the format string parser.
  void after_width() {
    CAF_ASSERT(width.has_value());
    if (align == 0)        // if alignment is set, we manage the width ourselves
      print(fstr, *width); // NOLINT(bugprone-unchecked-optional-access)
  }

  // Callback for the format string parser.
  void after_width_index(size_t index) {
    if (index >= args.size())
      CAF_RAISE_ERROR(std::logic_error,
                      "invalid format string: width index out of range");
    const auto& arg = args[index];
    if (const auto* i64 = std::get_if<int64_t>(&arg)) {
      if (*i64 < 0)
        CAF_RAISE_ERROR(std::logic_error,
                        "invalid format string: negative width");
      width = static_cast<size_t>(*i64);
      after_width();
      return;
    }
    if (const auto* u64 = std::get_if<uint64_t>(&arg)) {
      width = static_cast<size_t>(*u64);
      after_width();
      return;
    }
    CAF_RAISE_ERROR(std::logic_error,
                    "invalid format string: expected an integer for width");
  }

  // Callback for the format string parser.
  void after_precision(size_t value) {
    print(fstr, value);
  }

  // Callback for the format string parser.
  void after_precision_index(size_t index) {
    if (index >= args.size())
      CAF_RAISE_ERROR(std::logic_error,
                      "invalid format string: precision index out of range");
    const auto& arg = args[index];
    if (const auto* i64 = std::get_if<int64_t>(&arg)) {
      if (*i64 < 0)
        CAF_RAISE_ERROR(std::logic_error,
                        "invalid format string: negative precision");
      print(fstr, *i64);
      return;
    }
    if (const auto* u64 = std::get_if<uint64_t>(&arg)) {
      print(fstr, *u64);
      return;
    }
    CAF_RAISE_ERROR(std::logic_error,
                    "invalid format string: expected an integer for precision");
  }

  // Callback for the format string parser.
  void set_type(char c) {
    type = c;
    fstr.push_back(c);
  }

  // Callback for the format string parser.
  void set_padding(char c) {
    fstr.push_back(c);
    if (align != 0)
      CAF_RAISE_ERROR(std::logic_error, "invalid format string: padding and "
                                        "alignment are mutually exclusive");
  }

  // Callback for the format string parser.
  void set_align(char fill_char, char align_char) {
    fill = fill_char;
    align = align_char;
  }

  // Adds padding to the buffer and aligns the content.
  void apply_alignment(size_t fixed_width) {
    if (buf.size() < fixed_width) {
      if (align == 0)
        align = is_number ? '>' : '<';
      switch (align) {
        case '<':
          buf.insert(buf.end(), fixed_width - buf.size(), fill);
          break;
        case '>':
          buf.insert(buf.begin(), fixed_width - buf.size(), fill);
          break;
        default: // '^'
          buf.insert(buf.begin(), (fixed_width - buf.size()) / 2, fill);
          buf.resize(fixed_width, fill);
      }
    }
  }

  // Renders the next argument.
  void render() {
    if (arg_index) {
      render_arg(*arg_index);
      arg_index = std::nullopt;
    } else {
      render_arg(next_arg_index++);
    }
  }

  // Renders the argument at `index`.
  void render_arg(size_t index) {
    if (index >= args.size())
      CAF_RAISE_ERROR(std::logic_error, "index out of range");
    std::visit([&](auto val) { render_val(val); }, args[index]);
    apply_alignment(width.value_or(0));
  }

  // Writes "true" or "false" to the buffer.
  void render_val(bool val) {
    print(buf, val);
  }

  // Writes a single character to the buffer.
  void render_val(char val) {
    buf.push_back(val);
  }

  // Calls snprintf to render `val`.
  template <class T>
  void do_sprintf(T val) {
    fstr.push_back('\0');
    auto buf_size = std::snprintf(nullptr, 0, fstr.data(), val);
    if (buf_size <= 0)
      CAF_RAISE_ERROR(std::logic_error, "invalid format string");
    buf.resize(static_cast<size_t>(buf_size) + 1);
    buf_size = std::snprintf(buf.data(), buf_size + 1, fstr.data(), val);
    if (buf_size < 0 || static_cast<size_t>(buf_size) >= buf.size())
      CAF_RAISE_ERROR(std::logic_error, "snprintf failed");
    buf.resize(static_cast<size_t>(buf_size));
  }

  // Calls snprintf to render `val`.
  void render_val(int64_t val) {
    is_number = true;
    switch (type) {
      case 0:
        type = 'd';
        fstr.push_back('d');
        [[fallthrough]];
      case 'd':
      case 'i':
        fstr.pop_back();
        fstr.push_back('l');
        fstr.push_back('l');
        fstr.push_back(type);
        do_sprintf(static_cast<long long int>(val));
        break;
      case 'c':
        if (val >= -128 && val <= 127) {
          buf.push_back(static_cast<char>(val));
          return;
        }
        CAF_RAISE_ERROR(std::logic_error,
                        "cannot convert integer to character: too large");
      case 'o':
      case 'x':
      case 'X':
        if (val < 0)
          CAF_RAISE_ERROR(std::logic_error,
                          "cannot render negative number as unsigned");
        render_val(static_cast<uint64_t>(val));
        break;
      default:
        CAF_RAISE_ERROR(std::logic_error,
                        "invalid format string for unsigned integer");
    }
  }

  // Calls snprintf to render `val`.
  void render_val(uint64_t val) {
    is_number = true;
    switch (type) {
      case 'd':
        fstr.pop_back();
        [[fallthrough]];
      case 0:
        type = 'u';
        fstr.push_back('u');
        [[fallthrough]];
      case 'u':
      case 'o':
      case 'x':
      case 'X':
        fstr.pop_back();
        fstr.push_back('l');
        fstr.push_back('l');
        fstr.push_back(type);
        do_sprintf(static_cast<long long unsigned int>(val));
        break;
      case 'c':
        if (val <= 127) {
          buf.push_back(static_cast<char>(val));
          return;
        }
        CAF_RAISE_ERROR(std::logic_error,
                        "cannot convert integer to character: too large");
      default:
        CAF_RAISE_ERROR(std::logic_error,
                        "invalid format string for unsigned integer");
    }
  }

  // Calls snprintf to render `val`.
  void render_val(double val) {
    is_number = true;
    switch (type) {
      case 0:
        type = 'g';
        fstr.push_back('g');
        [[fallthrough]];
      case 'a':
      case 'A':
      case 'e':
      case 'E':
      case 'f':
      case 'F':
      case 'g':
      case 'G':
        do_sprintf(val);
        break;
      default:
        CAF_RAISE_ERROR(std::logic_error, "invalid type for floating point");
    }
  }

  // Writes a string to the buffer.
  void render_val(const char* val) {
    for (auto ch = *val++; ch != '\0'; ch = *val++)
      buf.push_back(ch);
  }

  // Writes a string to the buffer.
  void render_val(std::string_view val) {
    buf.insert(buf.end(), val.begin(), val.end());
  }

  // Writes a pointer (address) to the buffer.
  void render_val(const void* val) {
    print(buf, val);
  }
};

template <class ParserState>
void copy_verbatim(ParserState& ps, copy_state& cs);

// Parses a format string and writes the result to `cs`.
template <class ParserState>
void copy_formatted(ParserState& ps, copy_state& cs) {
  cs.reset();
  size_t tmp = 0;
  auto make_tmp_consumer = [&tmp] {
    tmp = 0;
    return make_consumer(tmp);
  };
  typename ParserState::iterator_type maybe_fill;
  // spec      =  [[fill]align][sign]["#"]["0"][width]["." precision]["L"][type]
  // fill      =  <a character other than '{' or '}'>
  // align     =  "<" | ">" | "^"
  // sign      =  "+" | "-" | " "
  // width     =  integer | "{" [arg_id] "}"
  // precision =  integer | "{" [arg_id] "}"
  // type      =  "a" | "A" | "b" | "B" | "c" | "d" | "e" | "E" | "f" | "F" |
  //              "g" | "G" | "o" | "p" | "s" | "x" | "X"
  // clang-format off
  start();
  state(init) {
    fsm_epsilon(read_unsigned_integer(ps, make_consumer(cs.arg_index)),
                after_arg_index, decimal_chars)
    epsilon(after_arg_index)
  }
  state(after_arg_index) {
    transition(has_close_brace, '}', cs.render())
    transition(read_format_spec, ':')
  }
  state(read_format_spec) {
    transition(disambiguate_fill, any_char, maybe_fill = ps.i)
  }
  state(disambiguate_fill) {
    transition(after_align, "<>^", cs.set_align(*maybe_fill, ch))
    epsilon(after_fill, any_char, ch = *(ps.i = maybe_fill)) // go back one char
  }
  state(after_fill) {
    transition(after_align, "<>^", cs.set_align(' ', ch))
    epsilon(after_align, any_char)
  }
  state(after_align) {
    transition(after_sign, "+- ", cs.fstr.push_back(ch))
    epsilon(after_sign, any_char)
  }
  state(after_sign) {
    transition(after_alt, '#', cs.fstr.push_back(ch))
    epsilon(after_alt, any_char)
  }
  state(after_alt) {
    transition(after_padding, '0', cs.set_padding(ch))
    epsilon(after_padding, any_char)
  }
  state(after_padding) {
    fsm_epsilon(read_unsigned_integer(ps, make_consumer(cs.width)),
                after_width_ret, decimal_chars)
    fsm_transition(read_unsigned_integer(ps, make_tmp_consumer()),
                   after_width_index, '{')
    epsilon(after_width, any_char)
  }
  state(after_width_index) {
    transition(after_width, '}', cs.after_width_index(tmp))
  }
  state(after_width_ret) {
    epsilon(after_width, any_char, cs.after_width())
  }
  state(after_width) {
    transition(read_precision, '.', cs.fstr.push_back(ch))
    epsilon(after_precision, any_char)
  }
  state(read_precision) {
    fsm_epsilon(read_unsigned_integer(ps, make_tmp_consumer()),
                after_precision_ret, decimal_chars)
    fsm_transition(read_unsigned_integer(ps, make_tmp_consumer()),
                   after_precision_index, '{')
    epsilon(after_precision, any_char)
  }
  state(after_precision_ret) {
    epsilon(after_width, any_char, cs.after_precision(tmp))
  }
  state(after_precision_index) {
    transition(after_precision, '}', cs.after_precision_index(tmp))
  }
  state(after_precision) {
    transition(after_locale, 'L') // not supported by snprintf
    epsilon(after_locale, any_char)
  }
  state(after_locale) {
    transition(after_type, "aAbBcdeEfFgGopsxX", cs.set_type(ch))
    transition(has_close_brace, '}', cs.render())
  }
  state(after_type) {
    transition(has_close_brace, '}', cs.render())
  }
  term_state(has_close_brace) {
    // nop
  }
  fin();
  // clang-format on
  if (ps.code <= pec::trailing_character)
    cs.fn = copy_verbatim<string_parser_state>;
}

// Copies the input verbatim to the output buffer until finding a format string.
template <class ParserState>
void copy_verbatim(ParserState& ps, copy_state& cs) {
  // clang-format off
  start();
  term_state(init) {
    transition(has_open_brace, '{')
    transition(has_close_brace, '}')
    transition(init, any_char, cs.buf.push_back(ch))
  }
  state(has_open_brace) {
    transition(init, '{', cs.buf.push_back('{'))
    epsilon(at_fmt_start, any_char, cs.fn = copy_formatted<string_parser_state>)
  }
  state(has_close_brace) {
    transition(init, '}', cs.buf.push_back('}'))
  }
  term_state(at_fmt_start) {
    // nop
  }
  fin();
  // clang-format on
}

copy_state::copy_state(span<format_arg> arg_list) : args(arg_list) {
  fn = copy_verbatim<string_parser_state>;
  buf.reserve(64);
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS

namespace caf::detail {

class compiled_format_string_impl : public compiled_format_string {
public:
  explicit compiled_format_string_impl(std::string_view fstr,
                                       span<format_arg> args)
    : begin_(fstr.begin()), state_(begin_, fstr.end()), copy_(args) {
    // nop
  }

  bool at_end() const noexcept override {
    return state_.at_end();
  }

  std::string_view next() override {
    copy_.buf.clear();
    copy_.fn(state_, copy_);
    if (state_.code > pec::trailing_character) {
      auto pos = std::distance(begin_, state_.i);
      std::string err = "format error at offset ";
      err += std::to_string(pos);
      err += ": ";
      err += to_string(state_.code);
      CAF_RAISE_ERROR(std::logic_error, err.c_str());
    }
    return std::string_view{copy_.buf.data(), copy_.buf.size()};
  }

private:
  std::string_view::iterator begin_;
  string_parser_state state_;
  parser::copy_state copy_;
};

compiled_format_string::~compiled_format_string() {
  // nop
}

std::unique_ptr<compiled_format_string>
compile_format_string(std::string_view fstr, span<format_arg> args) {
  return std::make_unique<compiled_format_string_impl>(fstr, args);
}

} // namespace caf::detail
