#include <string>
#include <vector>
#include <cctype>
#include <utility>
#include <cassert>
#include <fstream>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

using namespace caf;

using thread_id = string;
using vector_timestamp = std::vector<size_t>;

// -- convenience functions for strings

// removes leading and trailing whitespaces
void trim(string& s) {
  auto not_space = [](char c) { return isspace(c) == 0; };
  // trim left
  s.erase(s.begin(), find_if(s.begin(), s.end(), not_space));
  // trim right
  s.erase(find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

// -- convenience functions for I/O streams

using istream_fun = std::function<std::istream& (std::istream&)>;

std::istream& skip_whitespaces(std::istream& in) {
  while (in.peek() == ' ')
    in.get();
  return in;
}

std::istream& skip_to_next_line(std::istream& in) {
  in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  return in;
}

std::istream& skip_word(std::istream& in) {
  skip_whitespaces(in);
  auto nonspace = [](char x) { return (isprint(x) != 0) && (isspace(x) == 0); };
  while (nonspace(static_cast<char>(in.peek())))
    in.get();
  return in;
}

struct line_reader {
  std::string& line;
  char delim;
};

std::istream& operator>>(std::istream& in, line_reader x) {
  std::getline(in, x.line, x.delim);
  trim(x.line);
  return in;
}

line_reader rd_line(std::string& line, char delim = '\n') {
  return {line, delim};
}

struct istream_char_consumer {
  const char* what;
  size_t count;
};

std::istream& operator>>(std::istream& in, istream_char_consumer x) {
  if (!in)
    return in;
  // ignore leading whitespaces
  skip_whitespaces(in);
  // ignore trailing '\0'
  for (size_t i = 0; i < x.count; ++i) {
//cout << "in: " << (char) in.peek() << ", x: " << x.what[i] << endl;
    if (in.get() != x.what[i]) {
      in.setstate(std::ios::failbit);
      break;
    }
  }
  return in;
}

template <size_t S>
istream_char_consumer consume(const char (&what)[S]) {
  return {what, S - 1};
}

// -- convenience functions for vector timestamps

vector_timestamp& merge(vector_timestamp& x, const vector_timestamp& y) {
  assert(x.size() == y.size());
  for (size_t i = 0; i < x.size(); ++i)
    x[i] = std::max(x[i], y[i]);
  return x;
}

constexpr const char* log_level_name[] = {"ERROR", "WARN", "INFO",
                                          "DEBUG", "TRACE", "?????"};

enum class log_level { error, warn, info, debug, trace, invalid };

std::ostream& operator<<(std::ostream& out, const log_level& lvl) {
  return out << log_level_name[static_cast<size_t>(lvl)];
}

std::istream& operator>>(std::istream& in, log_level& lvl) {
  std::string tmp;
  in >> tmp;
  auto pred = [&](const char* cstr) {
    return cstr == tmp;
  };
  auto b = std::begin(log_level_name);
  auto e = std::end(log_level_name);
  auto i = std::find_if(b, e, pred);
  if (i == e)
    lvl = log_level::invalid;
  else
    lvl = static_cast<log_level>(std::distance(b, i));
  return in;
}

/// The ID of entities as used in a logfile. If the logger field is "actor0"
/// then this line represents a thread. Otherwise, the thread field is ignored.
struct logger_id {
  /// Content of the [LOGGER] field (0 if logger is a thread).
  actor_id aid;
  /// Content of the [THREAD] field.
  string tid;
};

bool operator<(const logger_id& x, const logger_id& y) {
  return x.aid == 0 && y.aid == 0 ? x.tid < y.tid : x.aid < y.aid;
}

std::istream& operator>>(std::istream& in, logger_id& x) {
  return in >> consume("actor") >> x.aid >> skip_whitespaces >> x.tid;
}

std::istream& operator>>(std::istream& in, node_id& x) {
  in >> skip_whitespaces;
  if (in.peek() == 'i') {
    x = node_id{};
    return in >> consume("invalid-node");
  }
  string node_hex_id;
  uint32_t pid;
  if (in >> rd_line(node_hex_id, '#') >> pid) {
    x = node_id{pid, node_hex_id};
  }
  return in;
}

/// The ID of a mailbox in a logfile. Parsed from `<actor>@<node>` entries.
struct mailbox_id {
  /// Actor ID of the receiver.
  actor_id aid;
  /// Node ID of the receiver.
  node_id nid;
};

std::string to_string(const mailbox_id& x) {
  auto res = std::to_string(x.aid);
  res += '@';
  res += to_string(x.nid);
  return res;
}

std::istream& operator>>(std::istream& in, mailbox_id& x) {
  // format is <actor>@<node>
  return in >> x.aid >> consume("@") >> x.nid;
}

std::ostream& operator<<(std::ostream& out, const mailbox_id& x) {
  return out << x.aid << '@' << to_string(x.nid);
}

/// An entity in our distributed system, i.e., either an actor or a thread.
struct entity {
  /// The ID of this entity if it is an actor, otherwise 0.
  actor_id aid;
  /// The ID of this entity if it is a thread, otherwise empty.
  thread_id tid;
  /// The ID of the node this entity is running at.
  node_id nid;
  /// The ID of this node in the vector clock.
  size_t vid;
  /// Marks system-level actors to enable filtering.
  bool hidden;
  /// A human-redable name, e.g., "actor42" or "thread23".
  string pretty_name;
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, entity& x) {
  return f(meta::type_name("entity"), x.aid, x.tid, x.nid, x.vid, x.hidden,
           x.pretty_name);
}

mailbox_id to_mailbox_id(const entity& x) {
  if (x.aid == 0)
    CAF_RAISE_ERROR("threads do not have a mailbox ID");
  return {x.aid, x.nid};
}

logger_id to_logger_id(const entity& x) {
  return {x.aid, x.tid};
}

/// Sorts entities by `nid` first, then places threads before actors
/// and finally compares `aid` or `tid`.
bool operator<(const entity& x, const entity& y) {
  // We sort by node ID first.
  auto cres = x.nid.compare(y.nid);
  if (cres != 0)
    return cres < 0;
  return (x.aid == 0 && y.aid == 0) ? x.tid < y.tid : x.aid < y.aid;
}

/// Set of `entity` sorted in ascending order by node ID, actor ID,
/// and thread ID (in that order).
using entity_set = std::set<entity>;

class entity_set_range {
public:
  using iterator = entity_set::const_iterator;

  entity_set_range() = default;
  entity_set_range(const entity_set_range&) = default;
  entity_set_range& operator=(const entity_set_range&) = default;

  iterator begin() const {
    return begin_;
  }

  iterator end() const {
    return end_;
  }

protected:
  iterator begin_;
  iterator end_;
};

struct node_cmp_t {
  bool operator()(const entity& x, const node_id& y) const {
    return x.nid < y;
  }
  bool operator()(const node_id& x, const entity& y) const {
    return x < y.nid;
  }
};

constexpr node_cmp_t node_cmp = node_cmp_t{};

/// Range within an `entity_set` containing all entities for a given actor.
struct actor_cmp_t {
  bool operator()(const entity& x, actor_id y) const {
    return x.aid < y;
  }
  bool operator()(actor_id x, const entity& y) const {
    return x < y.aid;
  }
};

constexpr actor_cmp_t actor_cmp = actor_cmp_t{};

class node_range : public entity_set_range {
public:
  node_range(const entity_set& xs, const node_id& y) {
    // get range for the node
    using namespace std;
    begin_ = lower_bound(xs.begin(), xs.end(), y, node_cmp);
    end_ = upper_bound(begin_, xs.end(), y, node_cmp);
  }

  node_range(const node_range&) = default;
  node_range& operator=(const node_range&) = default;

  const node_id& node() const {
    return node_;
  }

private:
  node_id node_;
};

/// Range within an `entity_set` containing all entities for a given node.
class thread_range : public entity_set_range {
public:
  thread_range(const node_range& xs) : node_(xs.node()) {
    actor_id dummy = 0;
    // get range for the node
    using namespace std;
    begin_ = xs.begin();
    end_ = upper_bound(begin_, xs.end(), dummy, actor_cmp);
  }

  thread_range(const thread_range&) = default;
  thread_range& operator=(const thread_range&) = default;

  const node_id& node() const {
    return node_;
  }

private:
  node_id node_;
};

const entity* get(const thread_range& xs, const thread_id& y) {
  // only compares thread ID
  auto thread_cmp = [](const entity& lhs, thread_id rhs) {
    return lhs.tid < rhs;
  };
  // range [xs.first, xs.second) is sortd by thread ID
  using namespace std;
  auto i = lower_bound(xs.begin(), xs.end(), y, thread_cmp);
  if (i->tid == y)
    return &(*i);
  return nullptr;
}

const entity* get(const node_range& xs, const thread_id& y) {
  thread_range subrange{xs};
  return get(subrange, y);
}

/// Returns the entity for `y` from the node range `xs`.
const entity* get(const node_range& xs, actor_id y) {
  if (y == 0)
    return nullptr;
  // range [xs.first, xs.second) is sortd by actor ID
  using namespace std;
  auto i = lower_bound(xs.begin(), xs.end(), y, actor_cmp);
  if (i->aid == y)
    return &(*i);
  return nullptr;
}

const entity* get(const node_range& xs, const logger_id& y) {
  return y.aid > 0 ? get(xs, y.aid) : get(xs, y.tid);
}

/// A single entry in a logfile.
struct log_entry {
  /// A UNIX timestamp.
  int64_t timestamp;
  /// Identifies the logging component, e.g., "caf".
  string component;
  /// Severity level of this entry.
  log_level level;
  /// ID of the logging entitiy.
  logger_id id;
  /// Context information about currently active class.
  string class_name;
  /// Context information about currently executed function.
  string function_name;
  /// Context information about currently executed source file.
  string file_name;
  /// Context information about currently executed source line.
  int32_t line_number;
  /// Description of the log entry.
  string message;
};

/// Stores a log event along with context information.
struct enhanced_log_entry {
  /// The original log entry without context information.
  const log_entry& data;
  /// The actual ID of the logging entity.
  const entity& id;
  /// Current vector time as seen by `id`.
  vector_timestamp& vstamp;
  /// JSON representation of `vstamp`.
  string json_vstamp;
};

/// CAF events according to SE-0001.
enum class se_type {
  spawn,
  init,
  send,
  reject,
  receive,
  drop,
  skip,
  finalize,
  terminate,
  none
};

string to_string(se_type x) {
  const char* tbl[] = {"spawn", "init", "send",     "reject",    "receive",
                       "drop",  "skip", "finalize", "terminate", "none"};
  return tbl[static_cast<int>(x)];
}

using string_map = std::map<string, string>;

/// An SE-0001 event, see http://actor-framework.github.io/rfcs/
struct se_event {
  const entity* source;
  vector_timestamp vstamp;
  se_type type;
  string_map fields;
};

string to_string(const se_event& x) {
  string res;
  res += "node{";
  res += to_string(*x.source);
  res += ", ";
  res += deep_to_string(x.vstamp);
  res += ", ";
  res += to_string(x.type);
  res += ", ";
  res += deep_to_string(x.fields);
  res += "}";
  return res;
}


CAF_ALLOW_UNSAFE_MESSAGE_TYPE(se_event)

bool field_key_compare(const std::pair<const std::string, std::string>& x,
                       const std::string& y) {
  return x.first == y;
}

#define ATM_CASE(name, value)                                                  \
  case static_cast<uint64_t>(atom(name)):                                      \
    y.type = se_type::value

#define CHECK_FIELDS(...)                                                      \
  {                                                                            \
    std::set<std::string> keys{__VA_ARGS__};                                   \
    if (y.fields.size() != keys.size())                                        \
      return sec::invalid_argument;                                            \
    if (!std::equal(y.fields.begin(), y.fields.end(), keys.begin(),            \
                    field_key_compare))                                        \
      return sec::invalid_argument;                                            \
  }                                                                            \
  static_cast<void>(0)

#define CHECK_NO_FIELDS()                                                      \
  if (!y.fields.empty())                                                       \
    return sec::invalid_argument;

expected<se_event> parse_event(const enhanced_log_entry& x) {
  se_event y{&x.id, x.vstamp, se_type::none, string_map{}};
  std::istringstream in{x.data.message};
  string type;
  if (!(in >> type))
    return sec::invalid_argument;
  string field_name;
  string field_content;
  in >> consume(";");
  while (in >> field_name >> consume("=") >> rd_line(field_content, ';'))
    y.fields.emplace(std::move(field_name), std::move(field_content));
  switch (static_cast<uint64_t>(atom_from_string(type))) {
    default:
      return sec::invalid_argument;
    ATM_CASE("SPAWN", spawn);
      CHECK_FIELDS("ID", "ARGS");
      break;
    ATM_CASE("INIT", init);
      CHECK_FIELDS("NAME", "HIDDEN");
      break;
    ATM_CASE("SEND", send);
      CHECK_FIELDS("TO", "FROM", "STAGES", "CONTENT");
      break;
    ATM_CASE("REJECT", reject);
      CHECK_NO_FIELDS();
      break;
    ATM_CASE("RECEIVE", receive);
      CHECK_FIELDS("FROM", "STAGES", "CONTENT");
      // insert TO field to allow comparing SEND and RECEIVE events easily
      y.fields.emplace("TO", to_string(to_mailbox_id(x.id)));
      break;
    ATM_CASE("DROP", drop);
      CHECK_NO_FIELDS();
      break;
    ATM_CASE("SKIP", skip);
      CHECK_NO_FIELDS();
      break;
    ATM_CASE("FINALIZE", finalize);
      CHECK_NO_FIELDS();
      break;
    ATM_CASE("TERMINATE", terminate);
      CHECK_FIELDS("REASON");
      break;
  }
  return {std::move(y)};
}

std::ostream& operator<<(std::ostream& out, const enhanced_log_entry& x) {
  return out << x.json_vstamp << ' ' << x.data.timestamp << ' '
             << x.data.component << ' ' << x.data.level << ' '
             << x.id.pretty_name << ' ' << x.data.class_name << ' '
             << x.data.function_name << ' '
             << x.data.file_name << ':' << x.data.line_number << ' '
             << x.data.message;
}

std::istream& operator>>(std::istream& in, log_entry& x) {
  in >> x.timestamp >> x.component >> x.level
     >> consume("actor") >> x.id.aid >> x.id.tid
     >> x.class_name >> x.function_name
     >> skip_whitespaces >> rd_line(x.file_name, ':')
     >> x.line_number >> skip_whitespaces >> rd_line(x.message);
  if (x.level == log_level::invalid)
    in.setstate(std::ios::failbit);
  return in;
}

struct logger_id_meta_data {
  bool hidden;
  string pretty_name;
};

/// Stores all log entities and their node ID.
struct first_pass_result {
  /// Node ID used in the parsed file.
  node_id this_node;
  /// Entities of the parsed file. The value is `true` if an entity is
  /// hidden, otherwise `false`.
  std::map<logger_id, logger_id_meta_data> entities;
};

enum verbosity_level {
  silent,
  informative,
  noisy
};

expected<first_pass_result> first_pass(blocking_actor* self, std::istream& in,
                                       verbosity_level vl) {
  first_pass_result res;
  // read first line to extract the node ID of local actors
  // _ caf INFO actor0 _ caf.logger start _:_ level = _, node = NODE
  if (!(in >> skip_word >> consume("caf") >> consume("INFO")
           >> consume("actor0") >> skip_word >> consume("caf.logger")
           >> consume("start") >> skip_word
           >> consume("level =") >> skip_word >> consume("node = ")
           >> res.this_node >> skip_to_next_line)) {
    cerr << "*** malformed log file, expect the first line to contain "
         << "an INFO entry of the logger" << endl;
    return sec::invalid_argument;
  }
  if (vl >= verbosity_level::informative)
    aout(self) << "found node " << res.this_node << endl;
  logger_id id;
  string message;
  while (in >> skip_word >> skip_word >> skip_word >> id
            >> skip_word >> skip_word >> skip_word >> rd_line(message)) {
    // store in map
    auto i = res.entities.emplace(id, logger_id_meta_data{false, "actor"}).first;
    if (starts_with(message, "INIT ; NAME = ")) {
      std::istringstream iss{message};
      iss >> consume("INIT ; NAME = ") >> rd_line(i->second.pretty_name, ';');
      if (ends_with(message, "HIDDEN = true"))
        i->second.hidden = true;
    }
  }
  if (vl >= verbosity_level::informative)
    aout(self) << "found " << res.entities.size() << " entities for node "
               << res.this_node << endl;
  return res;
}

const string& get(const std::map<string, string>& xs, const string& x) {
  auto i = xs.find(x);
  if (i != xs.end())
    return i->second;
  CAF_RAISE_ERROR("key not found");
}

void second_pass(blocking_actor* self, const group& grp,
                 const entity_set& entities, const node_id& nid,
                 const std::vector<string>& json_names, std::istream& in,
                 std::ostream& out, std::mutex& out_mtx,
                 bool drop_hidden_actors, verbosity_level vl) {
  assert(entities.size() == json_names.size());
  node_range local_entities{entities, nid};
  if (local_entities.begin() == local_entities.end())
    return;
  // state for each local entity
  struct state_t {
    const entity& eid;
    vector_timestamp clock;
  };
  std::map<logger_id, state_t> local_entities_state;
  for (auto& x : local_entities) {
    vector_timestamp vzero;
    vzero.resize(entities.size());
    local_entities_state.emplace(logger_id{x.aid, x.tid},
                                 state_t{x, std::move(vzero)});
  }
  // lambda for accessing state via logger ID
  auto state = [&](const logger_id& x) -> state_t& {
    auto i = local_entities_state.find(x);
    if (i != local_entities_state.end())
      return i->second;
    CAF_RAISE_ERROR("logger ID not found");
  };
  // additional state for second pass
  size_t line = 0;
  log_entry plain_entry;
  std::vector<se_event> in_flight_messages;
  std::vector<se_event> in_flight_spawns;
  // maps scoped actor IDs to their parent ID
  std::map<logger_id, logger_id> scoped_actors;
  // lambda for broadcasting events that could cross node boundary
  auto bcast = [&](const se_event& x) {
    if (vl >= verbosity_level::noisy)
      aout(self) << "broadcast event from " << nid
                 << ": " << deep_to_string(x) << endl;
    if (self != nullptr)
      self->send(grp, x);
  };
  // fetch message from another node via the group
  auto fetch_message = [&](const std::map<string, string>& fields)
                       -> se_event& {
    // TODO: this receive unconditionally waits on a message,
    //       i.e., is a potential deadlock
    if (vl >= verbosity_level::noisy)
      aout(self) << "wait for send from another node matching fields "
                 << deep_to_string(fields) << endl;
    se_event* res = nullptr;
    self->receive_while([&] { return res == nullptr; })(
      [&](const se_event& x) {
        switch (x.type) {
          default:
            break;
          case se_type::send:
            in_flight_messages.emplace_back(x);
            if (x.fields == fields)
              res = &in_flight_messages.back();
            break;
        }
      }
    );
    return *res;
  };
  // second pass
  while (in >> plain_entry) {
    ++line;
    // increment local time
    auto& st = state(plain_entry.id);
    // do not produce log output for internal actors but still track messages
    // through those actors, because they might be forwarding messages
    bool internal = drop_hidden_actors && st.eid.hidden;
    if (!internal)
      st.clock[st.eid.vid] += 1;
    // generate enhanced entry (with incomplete JSON timestamp for now)
    enhanced_log_entry entry{plain_entry, st.eid, st.clock, string{}};
    // check whether entry contains an SE-0001 event
    auto tmp = parse_event(entry);
    if (tmp) {
      auto& event = *tmp;
      switch (event.type) {
        default:
          break;
        case se_type::send:
          bcast(event);
          in_flight_messages.emplace_back(std::move(event));
          break;
        case se_type::receive: {
          auto pred = [&](const se_event& x) {
            assert(x.type == se_type::send);
            return event.fields == x.fields;
          };
          auto e = in_flight_messages.end();
          auto i = std::find_if(in_flight_messages.begin(), e, pred);
          if (i != e) {
            merge(st.clock, i->vstamp);
          } else {
            merge(st.clock, fetch_message(event.fields).vstamp);
          }
          break;
        }
        case se_type::spawn:
          in_flight_spawns.emplace_back(std::move(event));
          break;
        case se_type::init: {
          auto id_field = std::to_string(st.eid.aid);
          auto pred = [&](const se_event& x) {
            assert(x.type == se_type::spawn);
            return get(x.fields, "ID") == id_field;
          };
          auto e = in_flight_spawns.end();
          auto i = std::find_if(in_flight_spawns.begin(), e, pred);
          if (i != e) {
            merge(st.clock, i->vstamp);
            // keep book on scoped actors since their terminate
            // event propagates back to the parent
            if (get(event.fields, "NAME") == "scoped_actor")
              scoped_actors.emplace(plain_entry.id, to_logger_id(*i->source));
            in_flight_spawns.erase(i);
          } else {
            std::cerr << "*** cannot match init event to a previous spawn"
                      << endl;
          }
          break;
        }
        case se_type::terminate:
          auto i = scoped_actors.find(plain_entry.id);
          if (i != scoped_actors.end()) {
            // merge timestamp with parent to capture happens-before relation
            auto& parent_state = state(i->second);
            merge(parent_state.clock, st.clock);
            scoped_actors.erase(i);
          }
          break;
      }
    }
    // create ShiViz compatible JSON-formatted vector timestamp
    std::ostringstream oss;
    oss << '{';
    bool need_comma = false;
    for (size_t i = 0; i < st.clock.size(); ++i) {
      auto x = st.clock[i];
      if (x > 0) {
        if (need_comma)
          oss << ',';
        else
          need_comma = true;
        oss << '"' << json_names[i] << '"' << ':' << x;
      }
    }
    oss << '}';
    entry.json_vstamp = oss.str();
    // print entry to output file
    if (!internal) {
      std::lock_guard<std::mutex> guard{out_mtx};
      out << entry << '\n';
    }
  }
}

namespace {

struct config : public actor_system_config {
  string output_file;
  bool include_hidden_actors = false;
  size_t verbosity = 0;
  config() {
    opt_group{custom_options_, "global"}
    .add(output_file, "output-file,o", "Path for the output file")
    .add(include_hidden_actors, "include-hidden-actors,i",
         "Include hidden (system-level) actors")
    .add(verbosity, "verbosity,v", "Debug output (from 0 to 2)");
    // shutdown logging per default
    logger_verbosity = quiet_log_lvl_atom::value;
  }
};

// two pass parser for CAF log files that enhances logs with vector
// clock timestamps
void caf_main(actor_system& sys, const config& cfg) {
  using namespace std;
  if (cfg.output_file.empty()) {
    cerr << "*** no output file specified" << endl;
    return;
  }
  verbosity_level vl;
  switch (cfg.verbosity) {
    case 0:
      vl = silent;
      break;
    case 1:
      vl = verbosity_level::informative;
      break;
    default:
      vl = verbosity_level::noisy;
  }
  // open output file
  std::ofstream out{cfg.output_file};
  if (!out) {
    cerr << "unable to open output file: " << cfg.output_file << endl;
    return;
  }
  using file_path = string;
  static constexpr size_t irsize = sizeof(file_path) + sizeof(std::ifstream)
                                   + sizeof(first_pass_result);
  using ifstream_ptr = std::unique_ptr<std::ifstream>;
  struct intermediate_res {
    file_path fname;
    ifstream_ptr fstream;
    first_pass_result res;
    char pad[irsize >= CAF_CACHE_LINE_SIZE ? 1 : CAF_CACHE_LINE_SIZE - irsize];
    intermediate_res() = default;
    intermediate_res(intermediate_res&&) = default;
    intermediate_res& operator=(intermediate_res&&) = default;
    intermediate_res(file_path fp, ifstream_ptr fs, first_pass_result&& fr)
        : fname(std::move(fp)),
          fstream(std::move(fs)),
          res(std::move(fr)) {
      // nop
    }
  };
  // do a first pass on all files to extract node IDs and entities
  vector<intermediate_res> intermediate_results;
  intermediate_results.resize(cfg.args_remainder.size());
  for (size_t i = 0; i < cfg.args_remainder.size(); ++i) {
    auto& file = cfg.args_remainder.get_as<string>(i);
    auto ptr = &intermediate_results[i];
    ptr->fname = file;
    ptr->fstream.reset(new std::ifstream(file));
    if (!*ptr->fstream) {
      cerr << "could not open file: " << file << endl;
      continue;
    }
    sys.spawn([ptr, vl](blocking_actor* self) {
      auto& f = *ptr->fstream;
      auto res = first_pass(self, f, vl);
      if (res) {
        // rewind stream and push intermediate results
        f.clear();
        f.seekg(0);
        ptr->res = std::move(*res);
      }
    });
  }
  sys.await_all_actors_done();
  // post-process collected entity IDs before second pass
  entity_set entities;
  std::vector<string> entity_names;
  auto sort_pred = [](const intermediate_res& x, const intermediate_res& y) {
    return x.res.this_node < y.res.this_node;
  };
  std::map<string, size_t> pretty_actor_names;
  size_t thread_count = 0;
  // make sure we insert in sorted order into the entities set
  std::sort(intermediate_results.begin(), intermediate_results.end(),
            sort_pred);
  for (auto& ir : intermediate_results) {
    auto node_as_string = to_string(ir.res.this_node);
    for (auto& kvp : ir.res.entities) {
      string pretty_name;
      // make each (pretty) actor and thread name unique
      auto& pn = kvp.second.pretty_name;
      if (kvp.first.aid != 0)
        pretty_name = pn + std::to_string(++pretty_actor_names[pn]);
        //"actor" + std::to_string(kvp.first.aid);
      else
        pretty_name = "thread" + std::to_string(++thread_count);
      auto vid = entities.size(); // position in the vector timestamp
      entity_names.emplace_back(pretty_name);
      entities.emplace(entity{kvp.first.aid, kvp.first.tid, ir.res.this_node,
                              vid, kvp.second.hidden, std::move(pretty_name)});
    }
  }
  // check whether entities set is in the right order
  auto vid_cmp = [](const entity& x, const entity& y) {
    return x.vid < y.vid;
  };
  if (!std::is_sorted(entities.begin(), entities.end(), vid_cmp)) {
    cerr << "*** ERROR: entity set not sorted by vector timestamp ID:\n"
         << deep_to_string(entities) << endl;
    return;
  }
  // do a second pass for all log files
  // first line is the regex to parse the remainder of the file
  out << R"((?<clock>\S+) (?<timestamp>\d+) (?<component>\S+) )"
      << R"((?<level>\S+) (?<host>\S+) (?<class>\S+) (?<function>\S+) )"
      << R"((?<file>\S+):(?<line>\d+) (?<event>.+))"
      << endl;
  // second line is the separator for multiple runs
  out << endl;
  std::mutex out_mtx;
  auto grp = sys.groups().anonymous();
  for (auto& fpr : intermediate_results) {
    sys.spawn_in_group(grp, [&](blocking_actor* self) {
      second_pass(self, grp, entities, fpr.res.this_node, entity_names,
                  *fpr.fstream, out, out_mtx, !cfg.include_hidden_actors, vl);
    });
  }
  sys.await_all_actors_done();
}

} // namespace <anonymous>

CAF_MAIN()

