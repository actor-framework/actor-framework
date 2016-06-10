/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

static_assert(PY_MAJOR_VERSION > 3
              || (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 5),
              "CAF requires Python >= 3.5");

using namespace caf;
using namespace std;

constexpr char default_banner[] = R"__(
    _________   _____ __  __
   / ____/   | / ___// / / /
  / /   / /| | \__ \/ /_/ /   CAF
 / /___/ ___ |___/ / __  /   Shell
 \____/_/  |_/____/_/ /_/
)__";

namespace py = pybind11;

namespace {

class binding {
public:
  binding(std::string python_name, bool builtin_type)
      : python_name_(std::move(python_name)),
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

  virtual void append(message_builder& xs, py::handle x) const = 0;

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

  void append(message_builder& xs, py::handle x) const override {
    //xs.append(*reinterpret_cast<T*>(x.cast<void*>()));
    xs.append(x.cast<T>());
  }
};

class cpp_binding : public binding {
public:
  using binding::binding;

  virtual void register_type(pybind11::module& target) const = 0;

  virtual py::object to_object(message& xs, size_t pos) const = 0;
};

template <class T>
std::string py_repr(const T& x) {
  return deep_to_string(x);
}

std::string py_repr(const atom_value& x) {
  return "<atom:'" + to_string(x) + "'>";
}

template <class T>
class default_cpp_binding : public cpp_binding {
public:
  using cpp_binding::cpp_binding;

  void append(message_builder& xs, py::handle x) const override {
    /*
    auto typeinfo = py::detail::get_type_info(typeid(T));
    CAF_ASSERT(PyType_IsSubtype(Py_TYPE(x.ptr()), typeinfo->type));
    auto vptr = ((py::detail::instance<void>*) x.ptr())->value;
    xs.append(*reinterpret_cast<T*>(vptr));
    */
    xs.append(x.cast<T>());
  }

  void register_type(pybind11::module& target) const override {
    py::class_<T>(target, this->python_name().c_str())
    .def("__str__", &default_cpp_binding::str_impl, this->docstring().c_str())
    .def("__repr__", &default_cpp_binding::repr_impl, this->docstring().c_str());
  }

  py::object to_object(message& xs, size_t pos) const override {
    return py::object(
      py::detail::type_caster<typename py::detail::intrinsic_type<T>::type>::
        cast(xs.get_as<T>(pos), py::return_value_policy::automatic_reference,
             nullptr),
      false);
  }

private:
  static std::string str_impl(T* ptr) {
    return deep_to_string(*ptr);
  }

  static std::string repr_impl(T* ptr) {
    return py_repr(*ptr);
  }
};

using binding_ptr = std::unique_ptr<binding>;
using py_binding_ptr = std::unique_ptr<py_binding>;
using cpp_binding_ptr = std::unique_ptr<cpp_binding>;

atom_value atom_from_string(const std::string& str) {
  char buf[11];
  strncpy(buf, str.c_str(), std::min<size_t>(10, str.size()));
  buf[10] = '\0';
  return atom(buf);
}

class py_config : public actor_system_config {
public:
  std::string pre_run;
  std::string banner = default_banner;

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
    add_cpp<bool>("bool", "bool", true);
    add_cpp<float>("float", "float", true);
    add_cpp<int32_t>("int32_t", "@i32", true);
    add_cpp<std::string>("str", "@str", true);
  }

  template <class T>
  actor_system_config& add_message_type(std::string name) {
    add_cpp<T>(name, name);
    actor_system_config::add_message_type<T>(std::move(name));
  }

  void py_init(py::module& x) const {
    for (auto& kvp : cpp_bindings_)
      if (! kvp.second->builtin())
        kvp.second->register_type(x);
  }

  std::string ipython_script() const {
    // prepare preload script by formatting it with <space><space>'...'
    std::vector<std::string> lines;
    split(lines, pre_run, is_any_of("\n"), token_compress_on);
    for (auto& line : lines) {
      line.insert(0, "  '");
      line += "'";
    }
    std::ostringstream oss;
    oss << "import IPython" << endl
        << "c = IPython.Config()" << endl
        << "c.InteractiveShellApp.exec_lines = [" << endl
        <<   join(lines, "\n") << endl
        << "]" << endl
        << "c.PromptManager.in_template  = ' $: '" << endl
        << "c.PromptManager.in2_template = ' -> '" << endl
        << "c.PromptManager.out_template = ' >> '" << endl
        << "c.display_banner = True" << endl
        << "c.TerminalInteractiveShell.banner1 = \"\"\"" << endl
        << banner << endl
        << "\"\"\"" << endl
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
  void add_cpp(std::string py_name, std::string cpp_name, bool builtin = false) {
    auto ptr = new default_cpp_binding<T>(py_name, builtin);
    // all type names are prefix with "CAF."
    py_name.insert(0, "CAF.");
    cpp_bindings_.emplace(py_name, cpp_binding_ptr{ptr});
    bindings_.emplace(std::move(py_name), ptr);
    portable_bindings_.emplace(std::move(cpp_name), ptr);
  }

  std::unordered_map<std::string, cpp_binding*> portable_bindings_;

  std::unordered_map<std::string, binding*> bindings_;
  std::unordered_map<std::string, cpp_binding_ptr> cpp_bindings_;
  std::unordered_map<std::string, py_binding_ptr> py_bindings_;
};

struct py_context {
  const py_config& cfg;
  actor_system& system;
  scoped_actor& self;
};

py_context* s_context;

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

void py_send(py::args xs) {
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
      set_py_exception("Unable to add element of type \"",
                       type_name, "\" to message: type is unknown to CAF");
      return;
    }
    kvp->second->append(mb, *i);
  }
  s_context->self->send(dest, mb.move_to_message());
}

py::tuple py_dequeue() {
  auto& self = s_context->self;
  self->await_data();
  auto ptr = self->next_message();
  auto& msg = ptr->msg;
  auto& bindings = s_context->cfg.portable_bindings();
  py::tuple result(msg.size());
  for (size_t i = 0; i  < msg.size(); ++i) {
    auto rtti = msg.type(i);
    auto str_ptr = self->system().types().portable_name(rtti);
    if (! str_ptr) {
      set_py_exception("Unable to extract element #", i, " from message: ",
                       "could not get portable name of ", rtti.second->name());
      return {};
    }
    auto kvp = bindings.find(*str_ptr);
    if (kvp == bindings.end()) {
      set_py_exception("Unable to add element of type \"",
                       *str_ptr, "\" to message: type is unknown to CAF");
      return {};
    }
    auto obj = kvp->second->to_object(msg, i);
    PyTuple_SET_ITEM(result.ptr(), static_cast<int>(i), obj.release().ptr());
  }
  return result;
}

actor py_self() {
  return s_context->self;
}

class config : public py_config {
public:
  std::string py_file;

  config() {
    pre_run = "from CAF import *";
    opt_group{custom_options_, "global"}
    .add(py_file, "file,f", "Run script instead of interactive shell.");
  }
};

PyObject* caf_module_init() {
  py::module m("CAF", "Python binding for CAF");
  s_context->cfg.py_init(m);
  // add classes
  // add free functions
  m.def("send", &py_send, "Sends a message to an actor")
   .def("dequeue_message", &py_dequeue, "Receives the next message")
   .def("self", &py_self, "Returns the global self handle")
   .def("atom", &atom_from_string, "Creates an atom from a string");
  return m.ptr();
}

} // namespace <anonymous>

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
  if (! cfg.py_file.empty()) {
    auto fp = fopen(cfg.py_file.c_str() , "r");
    if (! fp) {
      cerr << "Unable to open file " << cfg.py_file << endl;
      Py_Finalize();
      return;
    }
    py_res = PyRun_SimpleString(cfg.pre_run.c_str());
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

CAF_MAIN()
