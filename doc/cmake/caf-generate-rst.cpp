/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <ctype.h>

#include <stack>

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_number_or_timespan.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/exec_main.hpp"
#include "caf/pec.hpp"
#include "caf/uri_builder.hpp"

namespace {

void trim(std::string& s) {
  auto not_space = [](char c) { return isspace(c) == 0; };
  s.erase(s.begin(), find_if(s.begin(), s.end(), not_space));
  s.erase(find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

template <class>
struct type_name;

template <class>
struct is_inline;

#define DECLARE_NAMED_STRUCT(type, inline_flag)                                \
  struct type;                                                                 \
  template <>                                                                  \
  struct type_name<type> {                                                     \
    static constexpr const char* value = #type;                                \
  };                                                                           \
  template <>                                                                  \
  struct is_inline<type> {                                                     \
    static constexpr bool value = inline_flag;                                 \
  }

DECLARE_NAMED_STRUCT(section, false);
DECLARE_NAMED_STRUCT(subsection, false);
DECLARE_NAMED_STRUCT(subsubsection, false);
DECLARE_NAMED_STRUCT(paragraph, false);
DECLARE_NAMED_STRUCT(label, false);
DECLARE_NAMED_STRUCT(see, true);
DECLARE_NAMED_STRUCT(sref, true);
DECLARE_NAMED_STRUCT(ref, true);
DECLARE_NAMED_STRUCT(verbatim, false);
DECLARE_NAMED_STRUCT(lstlisting, false);
DECLARE_NAMED_STRUCT(lstinline, true);
DECLARE_NAMED_STRUCT(text, true);
DECLARE_NAMED_STRUCT(texttt, true);
DECLARE_NAMED_STRUCT(textbf, true);
DECLARE_NAMED_STRUCT(textit, true);
DECLARE_NAMED_STRUCT(href, true);
DECLARE_NAMED_STRUCT(item, false);
DECLARE_NAMED_STRUCT(itemize, false);
DECLARE_NAMED_STRUCT(enumerate, false);
DECLARE_NAMED_STRUCT(tabular, false);
DECLARE_NAMED_STRUCT(cppexample, false);
DECLARE_NAMED_STRUCT(iniexample, false);
DECLARE_NAMED_STRUCT(sourcefile, false);
DECLARE_NAMED_STRUCT(singlefig, false);
DECLARE_NAMED_STRUCT(experimental, true);

using node = caf::variant<section, subsection, subsubsection, paragraph, label,
                          see, sref, ref, verbatim, lstlisting, lstinline, text,
                          texttt, textbf, textit, href, item, itemize,
                          enumerate, tabular, cppexample, iniexample,
                          sourcefile, singlefig, experimental>;

struct section {
  std::string name;
};

struct subsection {
  std::string name;
};

struct subsubsection {
  std::string name;
};

struct paragraph {
  std::string name;
};

struct label {
  std::string name;
};

struct see {
  std::string link;
};

struct sref {
  std::string link;
};

struct ref {
  std::string link;
};

struct verbatim {
  std::string block;
};

struct lstlisting {
  std::string block;
};

struct lstinline {
  std::string str;
};

struct text {
  std::string str;
};

struct texttt {
  std::string str;
};

struct textbf {
  std::string str;
};

struct textit {
  std::string str;
};

struct href {
  std::string url;
  std::string str;
};

struct item {
  std::vector<node> nodes;
};

struct itemize {
  std::vector<item> items;
};

struct enumerate {
  std::vector<item> items;
};

struct tabular {
  using column_type = item;

  using row_type = std::vector<column_type>;

  std::vector<row_type> rows;
};

struct cppexample {
  std::string lines;
  std::string file;
};

struct iniexample {
  std::string lines;
  std::string file;
};

struct sourcefile {
  std::string lines;
  std::string file;
};

struct singlefig {
  std::string file;
  std::string caption;
  std::string label;
};

struct experimental {
  // nop
};

#define MAKE_NODE_0(type)                                                      \
  if (name == #type) {                                                         \
    if (args.size() != 0)                                                      \
      throw std::runtime_error("expected exactly 0 argument for " #type        \
                               ", got: "                                       \
                               + caf::deep_to_string(args));                   \
    return type{};                                                             \
  }

#define MAKE_NODE_1(type)                                                      \
  if (name == #type) {                                                         \
    if (args.size() != 1)                                                      \
      throw std::runtime_error("expected exactly 1 argument for " #type        \
                               ", got: "                                       \
                               + caf::deep_to_string(args));                   \
    return type{std::move(args[0])};                                           \
  }

#define MAKE_NODE_2(type)                                                      \
  if (name == #type) {                                                         \
    if (args.size() != 2)                                                      \
      throw std::runtime_error("expected exactly 2 argument for " #type        \
                               ", got: "                                       \
                               + caf::deep_to_string(args));                   \
    return type{std::move(args[0]), std::move(args[1])};                       \
  }

#define MAKE_NODE_3(type)                                                      \
  if (name == #type) {                                                         \
    if (args.size() != 3)                                                      \
      throw std::runtime_error("expected exactly 3 argument for " #type        \
                               ", got: "                                       \
                               + caf::deep_to_string(args));                   \
    return type{std::move(args[0]), std::move(args[1]), std::move(args[2])};   \
  }

#define MAKE_LINES_AND_FILE_NODE(type)                                         \
  if (name == #type) {                                                         \
    if (args.size() == 1) {                                                    \
      return type{std::string{}, std::move(args[0])};                          \
    } else if (args.size() == 2) {                                             \
      return type{std::move(args[0]), std::move(args[1])};                     \
    }                                                                          \
    throw std::runtime_error("expected 1 or 2 arguments for " #type);          \
  }

node make_node(const std::string& name, std::vector<std::string> args) {
  MAKE_NODE_1(section)
  MAKE_NODE_1(subsection)
  MAKE_NODE_1(subsubsection)
  MAKE_NODE_1(paragraph)
  MAKE_NODE_1(label)
  MAKE_NODE_1(see)
  MAKE_NODE_1(sref)
  MAKE_NODE_1(ref)
  MAKE_NODE_1(verbatim)
  MAKE_NODE_1(lstlisting)
  MAKE_NODE_1(lstinline)
  MAKE_NODE_1(texttt)
  MAKE_NODE_1(textbf)
  MAKE_NODE_1(textit)
  MAKE_NODE_2(href)
  MAKE_LINES_AND_FILE_NODE(cppexample)
  MAKE_LINES_AND_FILE_NODE(iniexample)
  MAKE_LINES_AND_FILE_NODE(sourcefile)
  MAKE_NODE_3(singlefig)
  MAKE_NODE_0(experimental)
  if (name == "emph")
    return make_node("textit", std::move(args));
  throw std::runtime_error("unrecognized command: " + name
                           + caf::deep_to_string(args));
}

bool is_ignored_node(const std::string& name,
                     const std::vector<std::string>& args) {
  if (args.size() == 0)
    return name == "clearpage" || name == "textwidth";
  if (args.size() == 1)
    return (name == "begin" || name == "end")
           && (args[0] == "center" || args[0] == "footnotesize");
  return false;
}

struct abstract_consumer {
public:
  virtual ~abstract_consumer() {
    // nop
  }
  virtual void consume(node x) = 0;
};

template <class Result>
struct list_builder : abstract_consumer {
  using result_type = Result;

  abstract_consumer* consumer;
  Result result;
  bool finalized = false;

  explicit list_builder(abstract_consumer* ptr) : consumer(ptr) {
    // nop
  }

  void finalize() {
    consumer->consume(std::move(result));
    finalized = true;
  }

  void consume(node x) override {
    // The command parser might pass whitespaces to this builder after seeing
    // the end command.
    if (finalized) {
      consumer->consume(std::move(x));
      return;
    }
    if (result.items.empty())
      throw std::runtime_error("expected \\item as first token for list block");
    result.items.back().nodes.emplace_back(std::move(x));
  }

  void cmd(const std::string& name, std::vector<std::string> args) {
    if (name == "end" && args.size() == 1
        && args[0] == type_name<Result>::value) {
      finalize();
    } else if (name == "item") {
      result.items.emplace_back();
    } else {
      consumer->consume(make_node(name, std::move(args)));
    }
  }
};

struct tabular_builder : abstract_consumer {
  abstract_consumer* consumer;
  tabular result;
  bool finalized = false;

  explicit tabular_builder(abstract_consumer* consumer) : consumer(consumer) {
    next_row();
  }

  void consume(node x) override {
    // The command parser might pass whitespaces to this builder after seeing
    // the end command.
    if (finalized)
      consumer->consume(std::move(x));
    else
      result.rows.back().back().nodes.emplace_back(std::move(x));
  }

  void next_column() {
    result.rows.back().emplace_back();
  }

  void next_row() {
    result.rows.emplace_back();
    next_column();
  }

  void cmd(const std::string& name, std::vector<std::string> args) {
    if (name == "hline") {
      // drop
    } else if (name == "end" && args.size() == 1 && args[0] == "tabular") {
      if (result.rows.back().empty()) {
        result.rows.pop_back();
        if (result.rows.empty())
          throw std::runtime_error("empty table");
      }
      consumer->consume(std::move(result));
      finalized = true;
    } else {
      consume(make_node(name, std::move(args)));
    }
  }
};

} // namespace

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace {

using std::string;

using namespace caf;
using namespace caf::detail;
using namespace caf::detail::parser;

template <class State, class Consumer>
void read_tex_comment(State& ps, Consumer&&) {
  start();
  term_state(init){transition(done, '\n') transition(init)} term_state(done) {
    // nop
  }
  fin();
}

template <class State, class Consumer>
void read_tex_verbatim(State& ps, Consumer&& consumer, const string& cmd_name) {
  string verbatim;
  string cmd;
  string end_of_command = "end{" + cmd_name;
  auto flush_cmd = [&] {
    verbatim += '\\';
    verbatim += cmd;
    cmd.clear();
  };
  auto guard = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      std::vector<string> args;
      args.emplace_back(std::move(verbatim));
      consumer.cmd(cmd_name, std::move(args));
    }
  });
  // clang-format off
  start();
  state(init) {
    transition(read_end_verbatim, "\\");
    transition(init, any_char, verbatim += ch)
  }
  state(read_end_verbatim) {
    transition_if(cmd == end_of_command, done, "}")
    transition(init, "}", flush_cmd());
    transition(read_end_verbatim, "\\", flush_cmd());
    transition(read_end_verbatim, any_char, cmd += ch)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_tex_list(State& ps, Consumer&& consumer, const string& cmd_name);

template <class State, class Consumer>
void read_tex_tabular(State& ps, Consumer&& consumer);

template <class State, class Consumer>
void read_tex_command(State& ps, Consumer&& consumer) {
  string cmd;
  string spaces;
  std::vector<string> args;
  const char* stop_chars;
  auto add_arg = [&](const char* closing_char) {
    spaces.clear();
    stop_chars = closing_char;
    args.emplace_back();
  };
  auto is_verbatim_cmd = [&] {
    if (args.size() != 1)
      return false;
    return cmd == "begin" && (args[0] == "verbatim" || args[0] == "lstlisting");
  };
  auto is_list_cmd = [&] {
    if (args.size() != 1)
      return false;
    return cmd == "begin" && (args[0] == "itemize" || args[0] == "enumerate");
  };
  auto is_tabular_cmd = [&] {
    if (args.size() != 2)
      return false;
    return cmd == "begin" && args[0] == "tabular";
  };
  auto guard = make_scope_guard([&] {
    if (is_ignored_node(cmd, args))
      return;
    if (cmd.empty()) {
      ps.code = pec::unexpected_eof;
      return;
    }
    if (ps.code <= pec::trailing_character) {
      consumer.cmd(cmd, std::move(args));
      if (!spaces.empty())
        consumer.consume(text{std::move(spaces)});
    }
  });
  // clang-format off
  start();
  state(init) {
    epsilon(read_command)
  }
  term_state(read_command) {
    fsm_epsilon(read_tex_comment(ps, consumer), read_command, '%')
    fsm_epsilon_if(is_verbatim_cmd(), read_tex_verbatim(ps, consumer, args[0]), done, any_char)
    fsm_epsilon_if(is_list_cmd(), read_tex_list(ps, consumer, args[0]), done, any_char)
    fsm_epsilon_if(is_tabular_cmd(), read_tex_tabular(ps, consumer), done, any_char)
    transition_if(args.empty() && spaces.empty(), read_command, alphanumeric_chars, cmd += ch)
    transition(read_command_arg, "[", add_arg("]"))
    transition(read_command_arg, "{", add_arg("}"))
    transition(read_command_arg, "`", add_arg("`"))
    transition(read_command_arg, "^", add_arg("^"))
    transition(read_command, " \t\n", spaces += ch)
  }
  state(read_command_arg) {
    fsm_epsilon(read_tex_comment(ps, consumer), read_command_arg, '%')
    transition(read_command, stop_chars)
    transition(read_command_arg, any_char, args.back() += ch)
  }
  term_state(done) {
    guard.disable();
  }
  fin();
  // clang-format on
}

/// Reads an .tex formatted input file for the CAF manual.
template <class State, class Consumer>
void read_tex(State& ps, Consumer&& consumer) {
  string str;
  auto consume_str = [&] {
    if (!str.empty()) {
      consumer.consume(text{std::move(str)});
      str.clear();
    }
  };
  auto guard = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consume_str();
  });
  // clang-format off
  start();
  term_state(init) {
    fsm_transition(read_tex_comment(ps, consumer), init, '%')
    transition(start_escaping, "\\")
    transition(init, "~", str += ' ')
    transition(init, any_char, str += ch)
  }
  state(start_escaping) {
    transition(init, "\\", str += '\\')
    transition(init, "%", str += '%')
    fsm_epsilon(read_tex_command(ps, consumer), init, any_char, consume_str())
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_tex_list(State& ps, Consumer& consumer) {
  string str;
  auto consume_str = [&] {
    if (!str.empty()) {
      consumer.consume(text{std::move(str)});
      str.clear();
    }
  };
  auto before_first_item = [&] { return consumer.result.items.empty(); };
  // clang-format off
  start();
  state(init) {
    fsm_transition(read_tex_comment(ps, consumer), init, '%')
    transition(start_escaping, "\\")
    transition_if(!before_first_item(), init, "~", str += ' ')
    transition_if(!before_first_item(), init, any_char, str += ch)
    transition_if(before_first_item(), init, " \t\n")
  }
  state(start_escaping) {
    transition_if(!before_first_item(), init, "\\", str += '\\')
    transition_if(!before_first_item(), init, "%", str += '%')
    fsm_epsilon(read_tex_command(ps, consumer), after_cmd, any_char, consume_str())
  }
  unstable_state(after_cmd) {
    epsilon_if(consumer.finalized, done, any_char)
    epsilon(init, any_char)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_tex_list(State& ps, Consumer&& consumer, const string& cmd_name) {
  if (cmd_name == "itemize") {
    list_builder<itemize> builder{&consumer};
    read_tex_list(ps, builder);
  } else if (cmd_name == "enumerate") {
    list_builder<enumerate> builder{&consumer};
    read_tex_list(ps, builder);
  } else {
    throw std::logic_error("expected itemize or enumerate");
  }
}

template <class State, class Consumer>
void read_tex_tabular(State& ps, Consumer&& consumer) {
  tabular_builder builder{&consumer};
  string str;
  auto consume_str = [&] {
    if (!str.empty()) {
      builder.consume(text{str});
      str.clear();
    }
  };
  auto next_column = [&] {
    consume_str();
    builder.next_column();
  };
  auto next_row = [&] {
    consume_str();
    builder.next_row();
  };
  // clang-format off
  start();
  state(init) {
    fsm_transition(read_tex_comment(ps, builder), init, '%')
    transition(start_escaping, "\\")
    transition(init, "&", next_column())
    transition(init, "~", str += ' ')
    transition(init, any_char, str += ch)
  }
  state(start_escaping) {
    transition(init, "\\", next_row())
    transition(init, "%", str += '%')
    fsm_epsilon(read_tex_command(ps, builder), after_cmd, any_char, consume_str())
  }
  unstable_state(after_cmd) {
    epsilon_if(builder.finalized, done, any_char)
    epsilon(init, any_char)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

} // namespace

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS

namespace {

struct file_iter {
  std::istream* f;
  char ch;

  explicit file_iter(std::istream* f) : f(f) {
    f->get(ch);
  }

  file_iter() : f(nullptr), ch('\0') {
    // nop
  }

  file_iter(const file_iter&) = default;

  file_iter& operator=(const file_iter&) = default;

  char operator*() const {
    return ch;
  }

  file_iter& operator++() {
    f->get(ch);
    return *this;
  }
};

struct file_sentinel {};

bool operator!=(file_iter iter, file_sentinel) {
  return !iter.f->fail();
}

class rst_writer;

class rst_writer_state : public abstract_consumer {
public:
  rst_writer_state(rst_writer* parent) : parent_(parent) {
    // nop
  }

  virtual ~rst_writer_state() {
    // nop
  }

  virtual const char* name() const noexcept = 0;

  virtual void exit();

  rst_writer* parent() const noexcept {
    return parent_;
  };

  std::ostream& out();

protected:
  rst_writer* parent_;
};

struct parse_failure : std::runtime_error {
  using super = std::runtime_error;

  const char* state_name;

  parse_failure(const char* state_name, const std::string& what)
    : super(what), state_name(state_name) {
    // nop
  }

  template <class... Ts>
  [[noreturn]] static void raise(const char* state_name, const Ts&... xs) {
    throw parse_failure(state_name,
                        deep_to_string(std::forward_as_tuple(xs...)));
  }
};

template <class Subtype>
class rst_writer_state_base : public rst_writer_state {
public:
  using rst_writer_state::rst_writer_state;

  void consume(node x) override {
    current_node_ = &x;
    visit(dref(), x);
    current_node_ = nullptr;
  }

protected:
  template <class T, class... Ts>
  void epsilon(const std::unique_ptr<T>& target, Ts&&... xs) {
    auto& st = target->parent()->state;
    if (st)
      st->exit();
    st = target.get();
    target->entry(std::forward<Ts>(xs)...);
    target->consume(std::move(*current_node_));
  }

  template <class T>
  void unexpected(T&) {
    auto cstr = type_name<T>::value;
    parse_failure::raise(dref().name(), "unexpected command: ", cstr);
  }

private:
  Subtype& dref() {
    return static_cast<Subtype&>(*this);
  }

  node* current_node_ = nullptr;
};

template <class T, class... Ts>
void transition(const std::unique_ptr<T>& target, Ts&&... xs) {
  auto& st = target->parent()->state;
  if (st)
    st->exit();
  st = target.get();
  target->entry(std::forward<Ts>(xs)...);
}

#define DECLARE_STATE(name)                                                    \
  struct name##_state;                                                         \
  std::unique_ptr<name##_state> name;                                          \
  void make_##name()

class rst_writer : public abstract_consumer {
public:
  using state_ptr = std::unique_ptr<rst_writer_state>;

  rst_writer() : state(nullptr) {
    make_await_section();
    make_await_section_label();
    make_read_body();
    transition(await_section);
  }

  ~rst_writer() {
    state->exit();
  }

  void consume(node x) override {
    state->consume(std::move(x));
  }

  void cmd(const std::string& name, std::vector<std::string> args) {
    consume(make_node(name, std::move(args)));
  }

  std::string project_root;

  rst_writer_state* state;

  std::ofstream out;

  DECLARE_STATE(await_section);
  DECLARE_STATE(await_section_label);
  DECLARE_STATE(read_body);
};

std::ostream& rst_writer_state::out() {
  return parent_->out;
}

void rst_writer_state::exit() {
  // customization point
}

#define BEGIN_STATE(type)                                                      \
  struct rst_writer::type##_state                                              \
    : rst_writer_state_base<rst_writer::type##_state> {                        \
    using super = rst_writer_state_base<rst_writer::type##_state>;             \
    type##_state(rst_writer* parent) : super(parent) {                         \
    }                                                                          \
    const char* name() const noexcept override {                               \
      return #type;                                                            \
    }

#define END_STATE(type)                                                        \
  }                                                                            \
  ;                                                                            \
  void rst_writer::make_##type() {                                             \
    type.reset(new type##_state(this));                                        \
  }

BEGIN_STATE(await_section)

  void entry() {
    // nop
  }

  void operator()(section& x) {
    transition(parent_->await_section_label, x.name, '=');
  }

  template <class T>
  void operator()(T& x) {
    unexpected(x);
  }

END_STATE(await_section)

struct string_stream {
  std::string& result;
};

string_stream& operator<<(string_stream& out, string_view str) {
  out.result.insert(out.result.end(), str.begin(), str.end());
  return out;
}

string_stream& operator<<(string_stream& out, char c) {
  out.result += c;
  return out;
}

namespace rst_ops {

template <class Out>
Out& operator<<(Out& out, text& x) {
  // Trim all whitespaces on the left and right but one.
  if (x.str.empty())
    return out;
  auto no_space = [](char c) { return c != ' '; };
  auto i = std::find_if(x.str.begin(), x.str.end(), no_space);
  auto e = std::find_if(x.str.rbegin(), x.str.rend(), no_space).base();
  if (i > e)
    return out << ' ';
  if (i != x.str.begin())
    out << ' ';
  for (; i != e; ++i)
    out << *i;
  if (e != x.str.end())
    out << ' ';
  return out;
}

template <class Out>
Out& operator<<(Out& out, see& x) {
  return out << x.link << '_';
}

template <class Out>
Out& operator<<(Out& out, sref& x) {
  return out << x.link << '_';
}

template <class Out>
Out& operator<<(Out& out, ref& x) {
  return out << x.link << '_';
}

template <class Out>
Out& operator<<(Out& out, lstinline& x) {
  return out << "``" << x.str << "``";
}

template <class Out>
Out& operator<<(Out& out, texttt& x) {
  return out << "``" << x.str << "``";
}

template <class Out>
Out& operator<<(Out& out, textbf& x) {
  return out << "**" << x.str << "**";
}

template <class Out>
Out& operator<<(Out& out, textit& x) {
  return out << "*" << x.str << "*";
}

template <class Out>
Out& operator<<(Out& out, href& x) {
  return out << "`" << x.str << " <" << x.url << ">`_";
}

template <class Out>
Out& operator<<(Out& out, experimental&) {
  return out << "\\ :sup:`experimental`\\ ";
}

} // namespace rst_ops

template <class Out>
struct rst_ops_visitor : abstract_consumer {
  Out& out;

  rst_ops_visitor(Out& out) : out(out) {
    // nop
  }

  template <class T>
  detail::enable_if_t<is_inline<T>::value> operator()(T& x) {
    using namespace rst_ops;
    out << x;
  }

  template <class T>
  detail::enable_if_t<!is_inline<T>::value> operator()(T&) {
    throw std::runtime_error("expected an inline command");
  }

  void consume(node x) override {
    caf::visit(*this, x);
  }

  template <class T>
  void consume(T x) {
    (*this)(x);
  }

  void cmd(const std::string& name, std::vector<std::string> args) {
    consume(make_node(name, std::move(args)));
  }
};

BEGIN_STATE(await_section_label)

  void entry(const std::string& section_name, char highlighting) {
    spaces.clear();
    this->section_name.clear();
    this->highlighting = highlighting;
    // TODO: The tokenzier should parse TeX inside section names, too. Remove
    //       this hack once the sections contains nodes instead of just a
    //       string.
    string_stream str_out{this->section_name};
    rst_ops_visitor<string_stream> v{str_out};
    using iterator_type = std::string::const_iterator;
    parser_state<iterator_type> res{section_name.begin(), section_name.end()};
    read_tex(res, v);
  }

  void operator()(label& x) {
    out() << ".. _" << x.name << ":\n\n"
          << section_name << '\n'
          << std::string(section_name.size(), highlighting) << "\n\n";
    transition(parent_->read_body);
  }

  void operator()(text& x) {
    // Ignore whitespaces between \section and \label commands.
    if (x.str.empty())
      return;
    if (!spaces.empty()) {
      x.str.insert(x.str.begin(), spaces.begin(), spaces.end());
      delegate();
      return;
    }
    if (std::all_of(x.str.begin(), x.str.end(), ::isspace))
      spaces = std::move(x.str);
    else
      delegate();
  }

  template <class T>
  void operator()(T&) {
    delegate();
  }

  void delegate() {
    out() << section_name << '\n'
          << std::string(section_name.size(), highlighting) << "\n\n";
    epsilon(parent_->read_body);
  }

  std::string section_name;

  std::string spaces;

  char highlighting;

END_STATE(await_section_label)

BEGIN_STATE(read_body)

  void entry() {
    // nop
  }

  template <class T>
  detail::enable_if_t<is_inline<T>::value> operator()(T& x) {
    using namespace rst_ops;
    out() << x;
  }

  template <class T>
  detail::enable_if_t<!is_inline<T>::value> operator()(T& x) {
    unexpected(x);
  }

  void operator()(subsection& x) {
    transition(parent_->await_section_label, x.name, '-');
  }

  void operator()(subsubsection& x) {
    transition(parent_->await_section_label, x.name, '~');
  }

  void operator()(paragraph& x) {
    transition(parent_->await_section_label, x.name, '+');
  }

  void operator()(lstlisting& x) {
    print_block(".. code-block:: C++", x.block);
  }

  void operator()(verbatim& x) {
    print_block(".. ::", x.block);
  }

  void operator()(itemize& x) {
    out() << "\n\n";
    for (auto& i : x.items) {
      out() << "* ";
      for (auto& n : i.nodes)
        visit(*this, n);
      out() << '\n';
    }
    out() << '\n';
  }

  void operator()(enumerate& x) {
    size_t num = 1;
    out() << "\n\n";
    for (auto& i : x.items) {
      out() << num++ << ". ";
      for (auto& n : i.nodes)
        visit(*this, n);
      out() << '\n';
    }
    out() << '\n';
  }

  void operator()(tabular& x) {
    if (x.rows.empty() || x.rows[0].empty())
      throw std::runtime_error("empty tabular");
    // Convert the tabular into a string matrix.
    std::vector<std::vector<std::string>> content;
    content.reserve(x.rows.size());
    auto num_columns = x.rows[0].size();
    std::vector<size_t> column_sizes(num_columns);
    for (auto& row : x.rows) {
      // This hack makes sure we can handle \hline on the last line of a
      // tabular. It silently drops anything with different column side, but a
      // proper fix is not trivial.
      if (row.size() != num_columns)
        continue;
      content.emplace_back();
      auto& content_row = content.back();
      content_row.resize(num_columns);
      for (size_t col_index = 0; col_index < num_columns; ++col_index) {
        auto& cell = content_row[col_index];
        string_stream cell_out{cell};
        rst_ops_visitor<string_stream> v{cell_out};
        for (auto& node : row[col_index].nodes)
          visit(v, node);
        trim(cell);
        column_sizes[col_index] = std::max(column_sizes[col_index],
                                           cell.size());
      }
    }
    // Output the matrix.
    auto hline = [&] {
      for (auto cs : column_sizes) {
        out() << "+-";
        for (size_t i = 0; i < cs; ++i)
          out() << '-';
      }
      out() << "-+\n";
    };
    out() << "\n\n";
    hline();
    for (auto& row : content) {
      for (size_t col_index = 0; col_index < row.size(); ++col_index) {
        auto& column = row[col_index];
        column.resize(column_sizes[col_index], ' ');
        out() << "| " << column;
      }
      out() << " |\n";
      hline();
    }
    out() << '\n';
  }

  void operator()(cppexample& x) {
    auto path = parent_->project_root;
    path += "/examples/";
    path += x.file;
    path += ".cpp";
    std::ifstream in{path};
    print_file(".. code-block:: c++", in, x.lines);
  }

  void operator()(iniexample& x) {
    auto path = parent_->project_root;
    path += "/examples/";
    path += x.file;
    path += ".ini";
    std::ifstream in{path};
    print_file(".. code-block:: ini", in, x.lines);
  }

  void operator()(sourcefile& x) {
    auto path = parent_->project_root;
    path += '/';
    path += x.file;
    std::ifstream in{path};
    print_file(".. code-block:: c++", in, x.lines);
  }

  void operator()(singlefig& x) {
    out() << ".. _" << x.label << ":\n\n"
          << ".. image:: " << x.file << ".png" << '\n'
          << "   :alt: " << x.caption << "\n\n";
  }

  void print_block(const char* hdr, std::string& block) {
    // Trim leading and trailing newlines.
    while (block.size() > 0 && block.front() == '\n')
      block.erase(block.begin());
    while (block.size() > 0 && block.back() == '\n')
      block.pop_back();
    out() << "\n" << hdr << "\n\n";
    out() << "   ";
    for (auto ch : block) {
      if (ch == '\n')
        out() << "\n   ";
      else
        out() << ch;
    }
    out() << "\n\n";
  }

  void print_file(const char* hdr, std::ifstream& in, int first_line,
                  int last_line, int& line_num) {
    std::string line;
    while (line_num < first_line) {
      if (!std::getline(in, line))
        throw std::runtime_error("unexpected end of file");
      ++line_num;
    }
    out() << "\n" << hdr << "\n\n";
    while (line_num < last_line) {
      if (!std::getline(in, line))
        break;
      out() << "   " << line << '\n';
      ++line_num;
    }
    out() << "\n\n";
  }

  void print_file(const char* hdr, std::ifstream& in,
                  const std::string& lines) {
    int line_num = 1;
    if (lines.empty()) {
      print_file(hdr, in, 1, std::numeric_limits<int>::max(), line_num);
      return;
    }
    std::vector<std::string> line_ranges;
    split(line_ranges, lines, ",");
    for (const auto& lr : line_ranges) {
      std::vector<std::string> line_nums;
      split(line_nums, lr, "-");
      if (line_nums.size() != 2)
        throw std::runtime_error("illegal line range");
      print_file(hdr, in, std::stoi(line_nums[0]), std::stoi(line_nums[1]),
                 line_num);
    }
  }

END_STATE(read_body)

struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
      .add(input, "input,i", "input .tex file")
      .add(output, "output,o", "output .rst file")
      .add(project_root, "project-root,r", "project root for C++ examples");
  }

  std::string input;
  std::string output;
  std::string project_root;
};

} // namespace

int main(int argc, char** argv) {
  // Read CAF configuration.
  config cfg;
  if (auto err = cfg.parse(argc, argv)) {
    std::cerr << "unable to parse CAF config: " << cfg.render(err) << '\n';
    return EXIT_FAILURE;
  }
  if (cfg.cli_helptext_printed)
    return EXIT_SUCCESS;
  if (cfg.input.empty() || cfg.output.empty()) {
    std::cerr << "input or output path missing\n";
    return EXIT_FAILURE;
  }
  rst_writer consumer;
  consumer.out.open(cfg.output);
  if (!consumer.out) {
    std::cerr << "unable to open output file: " << cfg.output << '\n';
    return EXIT_FAILURE;
  }
  consumer.project_root = cfg.project_root;
  std::ifstream input{cfg.input};
  parser_state<file_iter, file_sentinel> res{file_iter{&input}};
  try {
    read_tex(res, consumer);
    if (res.i != res.e) {
      std::cerr << "error in line " << res.line << " on column " << res.column
                << ": " << to_string(res.code) << '\n';
      return EXIT_FAILURE;
    }
  } catch (parse_failure& x) {
    std::cerr << "error in line " << res.line << " on column " << res.column
              << " while in state " << x.state_name << ": " << x.what() << '\n';
    return EXIT_FAILURE;
  } catch (std::exception& x) {
    std::cerr << "error in line " << res.line << " on column " << res.column
              << ": " << x.what() << '\n';
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "unknown error in line " << res.line << " on column "
              << res.column << '\n';
    return EXIT_FAILURE;
  }
}
