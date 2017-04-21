/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#include <set>
#include <chrono>
#include <thread>
#include <iomanip>
#include <iostream>
#include <functional>
#include <unordered_map>

CAF_PUSH_WARNINGS
#include "third_party/pybind/include/pybind11/pybind11.h"
CAF_POP_WARNINGS

#include "caf/all.hpp"
#include "caf/io/all.hpp"

using std::cout;
using std::cerr;
using std::endl;

namespace {

constexpr char default_banner[] = R"__(
                  ____                __  __
      _________  / __/   ____  __  __/ /_/ /_  ____  ____
     / ___/ __ `/ /_____/ __ \/ / / / __/ __ \/ __ \/ __ `
    / /__/ /_/ / __/___/ /_/ / /_/ / /_/ / / / /_/ / / / /
    \___/\__,_/_/     / .___/\__, /\__/_/ /_/\____/_/ /_/
                     /_/    /____/

)__";

constexpr char init_script[] = R"__(
from CAF import *

caf_mail_cache=[]

def select_from_mail_cache(msg_filter):
    global caf_mail_cache
    for i, v in enumerate(caf_mail_cache):
        if msg_filter(v):
            return caf_mail_cache.pop(i)

def no_receive_filter(x):
    return True

def receive_one(abs_timeout):
    if abs_timeout:
        return dequeue_message_with_timeout(abs_timeout)
    else:
        return dequeue_message()

def receive(timeout = None, msg_filter = no_receive_filter):
    # try to get an element from the mailbox for predicate
    msg = select_from_mail_cache(msg_filter)
    if msg:
        return msg
    # calculate absolute timeout
    abs_timeout = None
    if timeout:
      abs_timeout = absolute_receive_timeout(int(timeout))
    # receive message via mailbox
    msg = receive_one(abs_timeout)
    while msg and not msg_filter(msg):
        caf_mail_cache.append(msg)
        msg = receive_one(abs_timeout)
    return msg

)__";

} // namespace <anonymous>

namespace caf {

void register_class(atom_value*, pybind11::module& m,
                    const std::string& name) {
  auto repr_fun = [](atom_value x) {
    return "atom('" + to_string(x) + "')";
  };
  auto cmp = [](atom_value x, atom_value y) {
    return x == y;
  };
  std::string (*to_string_fun)(const atom_value&) = &to_string;
  pybind11::class_<atom_value>(m, name.c_str())
  .def("__str__", to_string_fun)
  .def("__repr__", repr_fun)
  .def("__eq__", cmp);
}

namespace python {
namespace {

class binding {
public:
  binding(std::string py_name, bool builtin_type)
      : python_name_(std::move(py_name)),
        builtin_(builtin_type) {
    // nop
  }

  virtual ~binding() {
    //nop
  }

  inline void docstring(std::string x) {
    docstring_ = std::move(x);
  }

  inline const std::string& docstring() const {
    return docstring_;
  }

  inline const std::string& python_name() const {
    return python_name_;
  }

  inline bool builtin() const {
    return builtin_;
  }

  virtual void append(message_builder& xs, pybind11::handle x) const = 0;

private:
  std::string python_name_;
  std::string docstring_;
  bool builtin_;
};

class py_binding : public binding {
public:
  py_binding(std::string name) : binding(name, true) {
    // nop
  }
};

template <class T>
class default_py_binding : public py_binding {
public:
  using py_binding::py_binding;

  void append(message_builder& xs, pybind11::handle x) const override {
    xs.append(x.cast<T>());
  }
};

class cpp_binding : public binding {
public:
  using binding::binding;

  virtual pybind11::object to_object(const type_erased_tuple& xs,
                                     size_t pos) const = 0;
};

template <class T>
class default_cpp_binding : public cpp_binding {
public:
  using cpp_binding::cpp_binding;

  void append(message_builder& xs, pybind11::handle x) const override {
    xs.append(x.cast<T>());
  }

  pybind11::object to_object(const type_erased_tuple& xs,
                             size_t pos) const override {
    return pybind11::cast(xs.get_as<T>(pos));
  }
};

using binding_ptr = std::unique_ptr<binding>;
using py_binding_ptr = std::unique_ptr<py_binding>;
using cpp_binding_ptr = std::unique_ptr<cpp_binding>;

atom_value atom_from_string(const std::string& str) {
  static constexpr size_t buf_size = 11;
  char buf[buf_size];
  memset(buf, 0, buf_size);
  strncpy(buf, str.c_str(), std::min<size_t>(buf_size - 1, str.size()));
  return atom(buf);
}

template <class T>
class has_register_class {
private:
  template <class U>
  static auto test(U* x) -> decltype(register_class(x,
                                                    std::declval<pybind11::module&>(),
                                                    std::declval<const std::string&>()));

  static auto test(...) -> std::false_type;

  using type = decltype(test(static_cast<T*>(nullptr)));
public:
  static constexpr bool value = std::is_same<type, void>::value;
};

template <class T>
class has_to_string {
private:
  template <class U>
  static auto test(U* x) -> decltype(to_string(*x));

  static auto test(...) -> void;

  using type = decltype(test(static_cast<T*>(nullptr)));
public:
  static constexpr bool value = std::is_same<type, std::string>::value;
};

template <class T>
typename std::enable_if<
  !has_register_class<T>::value
  && has_to_string<T>::value
>::type
default_python_class_init(pybind11::module& m, const std::string& name) {
  auto str_impl = [](const T& x) {
    return to_string(x);
  };
  pybind11::class_<T>(m, name.c_str())
  .def("__str__", str_impl);
}

template <class T>
typename std::enable_if<
  !has_register_class<T>::value
  && !has_to_string<T>::value
>::type
default_python_class_init(pybind11::module& m, const std::string& name) {
  auto str_impl = [](const T& x) {
    return to_string(x);
  };
  pybind11::class_<T>(m, name.c_str());
}

template <class T>
typename std::enable_if<
  has_register_class<T>::value
>::type
default_python_class_init(pybind11::module& m, const std::string& name) {
  register_class(static_cast<T*>(nullptr), m, name);
}

struct absolute_receive_timeout {
public:
  using ms = std::chrono::milliseconds;
  using clock_type = std::chrono::high_resolution_clock;

  absolute_receive_timeout(int msec) {
    x_ = clock_type::now() + ms(msec);
  }

  absolute_receive_timeout() = default;
  absolute_receive_timeout(const absolute_receive_timeout&) = default;
  absolute_receive_timeout& operator=(const absolute_receive_timeout&) = default;

  const clock_type::time_point& value() const {
    return x_;
  }

  friend void serialize(serializer& sink, absolute_receive_timeout& x,
                        const unsigned int) {
    auto tse = x.x_.time_since_epoch();
    auto ms_since_epoch = std::chrono::duration_cast<ms>(tse).count();
    sink << static_cast<uint64_t>(ms_since_epoch);
  }

  friend void serialize(deserializer& source, absolute_receive_timeout& x,
                        const unsigned int) {
    uint64_t ms_since_epoch;
    source >> ms_since_epoch;
    clock_type::time_point y;
    y += ms(static_cast<ms::rep>(ms_since_epoch));
    x.x_ = y;
  }

private:
  clock_type::time_point x_;
};

void register_class(absolute_receive_timeout*, pybind11::module& m,
                    const std::string& name) {
  pybind11::class_<absolute_receive_timeout>(m, name.c_str())
  .def(pybind11::init<>())
  .def(pybind11::init<int>());
}

class py_config : public actor_system_config {
public:
  std::string pre_run;
  std::string banner = default_banner;

  using register_fun = std::function<void (pybind11::module&, const std::string&)>;

  py_config() {
    // allow CAF to convert native Python types to C++ types
    add_py<int>("int");
    add_py<bool>("bool");
    add_py<float>("float");
    add_py<std::string>("str");
    // create Python bindings for builtin CAF types
    add_cpp<actor>("actor", "@actor");
    add_cpp<message>("message", "@message");
    add_cpp<atom_value>("atom_value", "@atom");
    // fill list for native type bindings
    add_cpp<bool>("bool", "bool", nullptr);
    add_cpp<float>("float", "float", nullptr);
    add_cpp<int32_t>("int32_t", "@i32", nullptr);
    add_cpp<std::string>("str", "@str", nullptr);
    // custom types of caf_python
    add_message_type<absolute_receive_timeout>("absolute_receive_timeout");
  }

  template <class T>
  py_config&
  add_message_type(std::string name,
                   register_fun reg = &default_python_class_init<T>) {
    add_cpp<T>(name, name, std::move(reg));
    actor_system_config::add_message_type<T>(std::move(name));
    return *this;
  }

  void py_init(pybind11::module& x) const {
    for (auto& f : register_funs_)
      f(x);
  }

  std::string full_pre_run_script() const {
    return init_script + pre_run;
  }

  std::string ipython_script() const {
    // prepare preload script by formatting it with <space><space>'...'
    std::vector<std::string> lines;
    auto full_pre_run = full_pre_run_script();
    split(lines, full_pre_run, is_any_of("\n"), token_compress_on);
    for (auto& line : lines) {
      line.insert(0, "  '");
      line += "'";
    }
    std::ostringstream oss;
    oss << "import IPython" << endl
        << "c = IPython.Config()" << endl
        << "c.InteractiveShellApp.exec_lines = [" << endl
        << R"(""")"
        << full_pre_run
        << R"(""")" << endl
        << "]" << endl
        << "c.PromptManager.in_template  = ' $: '" << endl
        << "c.PromptManager.in2_template = ' -> '" << endl
        << "c.PromptManager.out_template = ' >> '" << endl
        << "c.display_banner = True" << endl
        << R"(c.TerminalInteractiveShell.banner1 = """)" << endl
        << banner << endl
        << R"(""")" << endl
        << "IPython.start_ipython(config=c)" << endl;
    return oss.str();
  }

   const std::unordered_map<std::string, binding*>& bindings() const {
     return bindings_;
   }

   const std::unordered_map<std::string, cpp_binding*>& portable_bindings() const {
     return portable_bindings_;
   }

   const std::unordered_map<std::string, cpp_binding_ptr>& cpp_bindings() const {
     return cpp_bindings_;
   }

private:
  template <class T>
  void add_py(std::string name) {
    auto ptr = new default_py_binding<T>(name);
    py_bindings_.emplace(name, py_binding_ptr{ptr});
    bindings_.emplace(std::move(name), ptr);
  }

  template <class T>
  void add_cpp(std::string py_name, std::string cpp_name,
               const register_fun& reg = &default_python_class_init<T>) {
    if (reg)
      register_funs_.push_back([=](pybind11::module& m) { reg(m, py_name); });
    auto ptr = new default_cpp_binding<T>(py_name, reg != nullptr);
    // all type names are prefix with "CAF."
    py_name.insert(0, "CAF.");
    cpp_bindings_.emplace(py_name, cpp_binding_ptr{ptr});
    bindings_.emplace(std::move(py_name), ptr);
    portable_bindings_.emplace(std::move(cpp_name), ptr);
  }

  template <class T>
  void add_cpp(std::string name) {
    add_cpp<T>(name, name);
  }

  std::unordered_map<std::string, cpp_binding*> portable_bindings_;
  std::unordered_map<std::string, binding*> bindings_;
  std::unordered_map<std::string, cpp_binding_ptr> cpp_bindings_;
  std::unordered_map<std::string, py_binding_ptr> py_bindings_;

  std::vector<std::function<void (pybind11::module&)>> register_funs_;
};
struct py_context {
  const py_config& cfg;
  actor_system& system;
  scoped_actor& self;
};

namespace {

py_context* s_context;

} // namespace <anonymous>

inline void set_py_exception_fill(std::ostream&) {
  // end of recursion
}

template <class T, class... Ts>
void set_py_exception_fill(std::ostream& oss, T&& x, Ts&&... xs) {
  set_py_exception_fill(oss << std::forward<T>(x), std::forward<Ts>(xs)...);
}


template <class... Ts>
void set_py_exception(Ts&&... xs) {
  std::ostringstream oss;
  set_py_exception_fill(oss, std::forward<Ts>(xs)...);
  PyErr_SetString(PyExc_RuntimeError, oss.str().c_str());
}

void py_send(const pybind11::args& xs) {
  if (xs.size() < 2) {
    set_py_exception("Too few arguments to call CAF.send");
    return;
  }
  auto i = xs.begin();
  auto dest = (*i).cast<actor>();
  ++i;
  message_builder mb;
  auto& bindings = s_context->cfg.bindings();
  for (; i != xs.end(); ++i) {
    std::string type_name = PyEval_GetFuncName((*i).ptr());
    auto kvp = bindings.find(type_name);
    if (kvp == bindings.end()) {
      set_py_exception(R"(Unable to add element of type ")",
                       type_name, R"(" to message: type is unknown to CAF)");
      return;
    }
    kvp->second->append(mb, *i);
  }
  s_context->self->send(dest, mb.move_to_message());
}

pybind11::tuple tuple_from_message(const type_erased_tuple& msg) {
  auto& self = s_context->self;
  auto& bindings = s_context->cfg.portable_bindings();
  pybind11::tuple result(msg.size());
  for (size_t i = 0; i  < msg.size(); ++i) {
    auto rtti = msg.type(i);
    auto str_ptr = self->system().types().portable_name(rtti);
    if (str_ptr == nullptr) {
      set_py_exception("Unable to extract element #", i, " from message: ",
                       "could not get portable name of ", rtti.second->name());
      return {};
    }
    auto kvp = bindings.find(*str_ptr);
    if (kvp == bindings.end()) {
      set_py_exception(R"(Unable to add element of type ")",
                       *str_ptr, R"(" to message: type is unknown to CAF)");
      return {};
    }
    auto obj = kvp->second->to_object(msg, i);
    PyTuple_SetItem(result.ptr(), static_cast<int>(i), obj.release().ptr());
  }
  return result;
}

pybind11::tuple py_dequeue() {
  auto& self = s_context->self;
  auto ptr = self->next_message();
  while (!ptr) {
    self->await_data();
    ptr = self->next_message();
  }
  return tuple_from_message(std::move(ptr->content()));
}

pybind11::tuple py_dequeue_with_timeout(absolute_receive_timeout timeout) {
  auto& self = s_context->self;
  auto ptr = self->next_message();
  while (!ptr) {
    if (!self->await_data(timeout.value()))
      return pybind11::none{};
    ptr = self->next_message();
  }
  return tuple_from_message(std::move(ptr->content()));
}

actor py_self() {
  return s_context->self;
}

struct foo {
  int x;
  int y;
  foo() : x(0), y(0) {
    // nop
  }
  foo(int a, int b) : x(a), y(b) {
    // nop
  }
};

template <class Processor>
void serialize(Processor& proc, foo& f, const unsigned int) {
  proc & f.x;
  proc & f.y;
}

std::string to_string(const foo& x) {
  return "foo" + deep_to_string_as_tuple(x.x, x.y);
}

void register_class(foo*, pybind11::module& m, const std::string& name) {
  std::string (*str_fun)(const foo&) = &to_string;
  pybind11::class_<foo>(m, name.c_str())
  .def(pybind11::init<>())
  .def(pybind11::init<int, int>())
  .def("__str__", str_fun)
  .def("__repr__", str_fun)
  .def_readwrite("x", &foo::x)
  .def_readwrite("y", &foo::y);
}

#if PY_MAJOR_VERSION == 3
#define CAF_MODULE_INIT_RES PyObject*
#define CAF_MODULE_INIT_RET(res) return res;
#else
#define CAF_MODULE_INIT_RES void
#define CAF_MODULE_INIT_RET(unused)
#endif

CAF_MODULE_INIT_RES caf_module_init() {
  pybind11::module m("CAF", "Python binding for CAF");
  s_context->cfg.py_init(m);
  // add classes
  // add free functions
  m.def("send", &py_send, "Sends a message to an actor")
   .def("dequeue_message", &py_dequeue, "Receives the next message")
   .def("dequeue_message_with_timeout", &py_dequeue_with_timeout, "Receives the next message")
   .def("self", &py_self, "Returns the global self handle")
   .def("atom", &atom_from_string, "Creates an atom from a string");
  CAF_MODULE_INIT_RET(m.ptr())
}


} // namespace <anonymous> 
} // namespace python
} // namespace caf

namespace {

using namespace caf;
using namespace caf::python;

class config : public py_config {
public:
  std::string py_file;

  config() {
    add_message_type<foo>("foo");
    opt_group{custom_options_, "python"}
    .add(py_file, "file,f", "Run script instead of interactive shell.");
  }
};

void caf_main(actor_system& system, const config& cfg) {
  // register system and scoped actor in global variables
  scoped_actor self{system};
  py_context ctx{cfg, system, self};
  s_context = &ctx;
  // init Python
  PyImport_AppendInittab("CAF", caf_module_init);
  Py_Initialize();
  // create Python module for CAF
  int py_res = 0;
  if (!cfg.py_file.empty()) {
    auto fp = fopen(cfg.py_file.c_str() , "r");
    if (fp == nullptr) {
      cerr << "Unable to open file " << cfg.py_file << endl;
      Py_Finalize();
      return;
    }
    auto full_pre_run = cfg.full_pre_run_script();
    py_res = PyRun_SimpleString(full_pre_run.c_str());
    if (py_res == 0)
      py_res = PyRun_SimpleFileEx(fp, cfg.py_file.c_str(), 1);
  } else {
    auto script = cfg.ipython_script();
    py_res = PyRun_SimpleString(script.c_str());
  }
  if (py_res != 0) {
    cerr << "Unable to launch interactive Python shell!" << endl
         << "Please install it using: pip install ipython" << endl;
  }
  Py_Finalize();
}

} // namespace <anonymous>

CAF_MAIN()
