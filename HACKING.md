This document specifies the coding style for the C++ Actor Framework.
The style is loosely based on the
[Google C++ Style Guide](http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml)
and the coding conventions used by the STL.

Example for the Impatient
=========================

```cpp
// libcaf_example/caf/example/my_class.hpp

#ifndef CAF_EXAMPLE_MY_CLASS_HPP
#define CAF_EXAMPLE_MY_CLASS_HPP

#include <string>

namespace caf {
namespace example {

/**
 * This class is only being used as style guide example.
 */
class my_class {
 public:
  /**
   * Brief description. More description. Note that CAF uses the
   * "JavaDoc-style" autobrief option, i.e., everything up until the
   * first dot is the brief description.
   */
  my_class();
  
  /**
   * Destructs `my_class`. Please use Markdown in comments.
   */
  ~my_class();

  // do not use the @return if you start the brief description with "Returns"
  /**
   * Returns the name of this instance.
   */
  inline const std::string& name() const {
    return m_name;
  }
  
  /**
   * Sets the name of this instance.
   */
  inline void name(std::string new_name) {
    m_name = std::move(new_name);
  }
  
  /**
   * Prints the name to `STDIN`.
   */
  void print_name() const;

  /**
   * Does something (maybe).
   */
  void do_something();

 private:
  std::string m_name;
};

} // namespace example
} // namespace caf

#endif // CAF_EXAMPLE_MY_CLASS_HPP
```

```cpp
// libcaf_example/src/my_class.cpp

#include "caf/example/my_class.hpp"

#include <iostream>

namespace caf {
namespace example {

namespace {

constexpr const char default_name[] = "my object";

} // namespace <anonymous>

my_class::my_class() : m_name(default_name) {
  // nop
}

my_class::~my_class() {
  // nop
}

void my_class::print_name() const {
  std::cout << name() << std::endl;
}

void my_class::do_something() {
  if (name() == default_name) {
    std::cout << "You didn't gave me a proper name, so I "
              << "refuse to do something."
              << std::endl;
  } else {
    std::cout << "You gave me the name " << name() 
              << "... Do you really think I'm willing to do something "
                 "for you after insulting me like that?"
              << std::endl;
  }
}

} // namespace example
} // namespace caf

```


General rules
=============

- Use 2 spaces per indentation level.

- The maximum number of characters per line is 80.

- Never use tabs.

- Vertical whitespaces separate functions and are not used inside functions:
  use comments to document logical blocks.

- Header filenames end in `.hpp`, implementation files end in `.cpp`.

- Never declare more than one variable per line.

- `*` and `&` bind to the *type*, e.g., `const std::string& arg`.

- Namespaces do not increase the indentation level.

- Access modifiers, e.g. `public`, are indented one space.

- Use the order `public`, `protected`, and then `private`. 

- Always use `auto` to declare a variable unless you cannot initialize it
  immediately or if you actually want a type conversion. In the latter case,
  provide a comment why this conversion is necessary.

- Never use unwrapped, manual resource management such as `new` and `delete`.

- Do not use `typedef`. The syntax provided by `using` is much cleaner.

- Use `typename` only when referring to dependent names.

- Keywords are always followed by a whitespace: `if (...)`, `template <...>`,
  `while (...)`, etc.

- Always use `{}` for bodies of control structures such as `if` or `while`,
  even for bodies consiting only of a single statement.

- Opening braces belong to the same line:
  ```cpp

  if (my_condition) {
    my_fun();
  } else {
    my_other_fun();
  }
  ```

- Use standard order for readability: C standard libraries, C++ standard
  libraries, other libraries, (your) CAF headers:
  ```cpp
  
  #include <sys/types.h>

  #include <vector>
  
  #include "some/other/library.hpp"
  
  #include "caf/example/myclass.hpp"
  ```

- When declaring a function, the order of parameters is: outputs, then inputs.
  This follows the parameter order from the STL.

- Never use C-style casts.


Naming
======

- Class names, constants, and function names are all-lowercase with underscores.

- Types and variables should be nouns, while functions performing an action
  should be "command" verbs. Classes used to implement metaprogramming
  functions also should use verbs, e.g., `remove_const`.

- Member variables use the prefix `m_`.

- Thread-local variables use the prefix `t_`.

- Static, non-const variables are declared in the anonymous namespace 
  and use the prefix `s_`.

- Template parameter names use CamelCase.

- Getter and setter use the name of the member without the `m_` prefix:
  ```cpp
  
  class some_fun {
   public:
    // ...
    int value() const {
      return m_value;
    }
    void value(int new_value) {
      m_value = new_value;
    }
   private:
    int m_value;
  };
  ```


Headers
=======

- Each `.cpp` file has an associated `.hpp` file.
  Exceptions to this rule are unit tests and `main.cpp` files.

- Each class has its own pair of header and implementation
  files and the relative path for the header file is derived from its full name.
  For example, the header file for `caf::example::my_class` of `libcaf_example`
  is located at `libcaf_example/caf/example/my_class.hpp` and the
  source file at `libcaf_example/src/my_class.cpp`.

- All header files should use `#define` guards to prevent multiple inclusion. 
  The symbol name is `<RELATIVE>_<PATH>_<TO>_<FILE>_HPP`.

- Do not `#include` when a forward declaration suffices.

- Each library component must provide a `fwd.hpp` header providing forward
  declartations for all types used in the user API.

- Each library component must provide an `all.hpp` header that contains the
  main page for the documentation and includes all headers for the user API.

- Use `inline` for small functions (rule of thumb: 10 lines or less).


Breaking Statements
===================

- Break constructor initializers after the comma, use four spaces for
  indentation, and place each initializer on its own line (unless you don't
  need to break at all):
  ```cpp

  my_class::my_class()
      : my_base_class(some_function()),
        m_greeting("Hello there! This is my_class!"),
        m_some_bool_flag(false) {
    // ok
  }
  other_class::other_class() : m_name("tommy"), m_buddy("michael") {
    // ok
  }
  ```

- Break function arguments after the comma for both declaration and invocation:
  ```cpp

  intptr_t channel::compare(const abstract_channel* lhs,
                            const abstract_channel* rhs) {
    // ...
  }
  ```

- Break before tenary operators and before binary operators:
  ```cpp

  if (today_is_a_sunny_day()
      && it_is_not_too_hot_to_go_swimming()) {
    // ...
  }
  ```


Template Metaprogramming
========================

Despite its power, template metaprogramming came to the language pretty
much by accident. Templates were never meant to be used for compile-time
algorithms and type transformations. This is why C++ punishes metaprogramming
with an insane amount of syntax noise. In CAF, we make excessive use of
templates. To keep the code readable despite all the syntax noise, we have some
extra rules for formatting metaprogramming code.

- Brake `using name = ...` statements always directly after `=` if it
  does not fit in one line.

- Consider the *semantics* of a metaprogramming function. For example,
  `std::conditional` is an if-then-else construct. Hence, place the if-clause
  on its own line and do the same for the two cases.

- Use one level of indentation per "open" template and place the closing `>`,
  `>::type` or `>::value` on its own line. For example:
  ```cpp
  
  using optional_result_type = 
    typename std::conditional<
      std::is_same<result_type, void>::value,
      bool,
      optional<result_type>
    >::type;
  // think of it as the following (not valid C++):
  auto optional_result_type = 
    conditional {
      if   result_type == void
      then bool
      else optional<result_type>
    };
  ```

- Note that this is not necessary when simply defining a type alias.
  When dealing with "ordinary" templates, indenting based on the position of
  the opening `<` is ok, e.g.:
  ```cpp

  using response_handle_type = response_handle<Subtype, message,
                                               ResponseHandleTag>;
  ```


Rvalue references
=================

- Use rvalue references whenever it makes sense.

- Write move constructors and assignment operators if possible.


Preprocessor Macros
===================

- Use macros if and only if you can't get the same result by using inline
  functions or proper constants.

- Macro names use the form `CAF_<COMPONENT>_<NAME>`.
