Git Workflow
============

Please adhere to the following Git naming and commit message conventions. Having
a consistent work flow and style reduces friction and makes organizing
contributions a lot easier for all sides.

Branches
--------

- Our main branch is `master`. It reflects the latest development changes for
  the next release and should always compile. Nightly versions use the `master`
  branch. Users looking for a production-ready state are encouraged to use the
  latest release version.

- As an external contributor, please fork the project on GitHub and create a
  pull request for your proposed changes. Otherwise, follow the guidelines
  below.

- After a GitHub issue has been assigned to you, create a branch with naming
  convention `issue/$num`, where `$num` is the issue ID on GitHub. Once you are
  ready, create a pull request for your branch. You can also file a draft pull
  request to get early feedback. In the pull request, put "Closes #$num." or
  "Fixes #$num." into the description if the pull request resolves the issue. If
  the pull request only partially addresses the issue, use "Relates #$num."

- For smaller changes that do not have a GitHub issue, we use topic branches
  with naming convention `topic/$user/short-description`. For example, a branch
  for fixing some typos in the documentation by user `johndoe` could be named
  `topic/johndoe/fix-doc-typos`.

Commit Message Style
--------------------

- Summarize the changes in no more than 50 characters in the first line.
  Capitalize and write in an imperative present tense, e.g., "Fix bug" as
  opposed to "Fixes bug" or "Fixed bug".

- Suppress the dot at the end of the first line. Think of it as the header of
  the following description.

- Leave the second line empty.

- Optionally add a long description written in full sentences beginning on the
  third line. Indent at 72 characters per line.

Pull Requests
-------------

- Squash your commits into a single one before filing a pull request if
  necessary. Each commit should represent a coherent set of changes.

- Wait for the results of our CI and fix all issues. Our CI builds CAF with GCC,
  Clang, and MSVC on various platforms and makes sure that the code is formatted
  correctly. We also measure code coverage and require that new code is covered
  by tests.

- We require at least one code review approval before merging a pull request. We
  take the review phase seriously and have a high bar for code quality. Multiple
  rounds of review are common.

- In case there are multiple rounds of review, please do not squash the commits
  until everything is resolved. This makes it easier for reviewers to see what
  has changed since the last review.

- A usual commit message for the review phase is "Integrate review feedback" or
  "Address review feedback" if you only changed the code according to the review
  recommendations.

- A maintainer will merge the pull request when all issues are resolved and the
  pull request has been approved by at least one reviewer. If there have been
  multiple rounds of review, the maintainer may squash the commits.

Coding Style
============

When contributing source code, please adhere to the following coding style,
which is loosely based the C++ Core Guidelines and the coding conventions used
by the C++ Standard Library.

Example for the Impatient
-------------------------

```c++
// File: libcaf_foo/caf/foo/my_class.hpp

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

// use "//" for regular comments and "///" for doxygen

namespace caf::foo {

/// This class is only being used as style guide example.
class my_class {
public:
  /// Brief description. More description. Note that CAF uses the
  /// "JavaDoc-style" autobrief option, i.e., everything up until the
  /// first dot is the brief description.
  my_class();

  /// Destructs `my_class`. Please use Markdown in comments.
  ~my_class();

  // omit redundant @return if you start the brief description with "Returns"
  /// Returns the name of this instance.
  const std::string& name() const noexcept {
    return name_;
  }

  /// Sets the name of this instance.
  void name(const std::string& new_name) {
    name_ = std::move(new_name);
  }

  /// Prints the name to `std::cout`.
  void print_name() const;

private:
  std::string name_; // Private member variables end with an underscore.
};

} // namespace caf::foo
```

```c++
// File: libcaf_foo/caf/foo/my_class.cpp

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/foo/my_class.hpp"

#include <iostream>
#include <string_view>

namespace caf::foo {

namespace {

constexpr std::string_view default_name = "my object";

} // namespace

my_class::my_class() : name_(default_name) {
  // nop
}

my_class::~my_class() {
  // nop
}

void my_class::print_name() const {
  std::cout << name() << std::endl;
}

} // namespace caf::foo
```

```c++
// File: libcaf_foo/caf/foo/my_class.test.cpp

// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/example/my_class.hpp" // the header-under-test

#include "caf/test/caf_test_main.hpp"
#include "caf/test/test.hpp"

TEST("my_class::name gets or sets the name") {
  caf::foo::my_class x;
  check_eq(x.name(), "my object");
  x.name("new name");
  check_eq(x.name(), "new name");
}

CAF_TEST_MAIN()
```

General
-------

Source code must be formatted using `clang-format` with the configuration file
`.clang-format` in the root directory of the repository. The following rules are
not enforced by `clang-format` and must be applied manually:

- Never use C-style casts.

- Never declare more than one variable per line.

- Only separate functions with vertical whitespaces and use comments to
  document logical blocks inside functions.

- Use `.hpp` as suffix for header files, `.cpp` as suffix for implementation
  files and `.test.cpp` as suffix for unit tests.

- Use `#include "caf/foo/bar.hpp"` for CAF headers and `#include <foo/bar.hpp>`
  for third-party headers.

- Use the order `public`, `protected`, and then `private` in classes.

- Always use `auto` to declare a variable unless you cannot initialize it
  immediately or if you actually want a type conversion. In the latter case,
  provide a comment why this conversion is necessary.

- Never use unwrapped, manual resource management such as `new` and `delete`,
  except when implementing a smart pointer or a similar abstraction.

- Prefer algorithms over manual loops.

- Prefer `using T = X` over `typedef X T`.

- Protect single-argument constructors with `explicit` to avoid implicit
  conversions.

- Use `noexcept` and attributes such as `[[nodiscard]]` whenever it makes sense
  and as long as it does not limit future design space. For example, move
  construction and assignment are natural candidates for `noexcept`.

Naming
------

- All names except macros and template parameters should be lower case and
  delimited by underscores, i.e., `snake_case`.

- Template parameter names should be written in `CamelCase`.

- Types and variables should be nouns, while functions performing an action
  should be "command" verbs. Classes used to implement metaprogramming functions
  also should use verbs, e.g., `remove_const`.

- Private and protected member variables use the suffix `_` while getter *and*
  setter functions use the name without suffix:

  ```c++
  class person {
  public:
    person(std::string name) : name_(std::move(name)) {
      // nop
    }

    const std::string& name() const noexcept {
      return name_
    }

    void name(const std::string& new_name) {
      name_ = new_name;
    }

  private:
    std::string name_;
  };
  ```

Headers
-------

- All header files use `#pragma once` to prevent multiple inclusion.

- Each library component should provide a `fwd.hpp` header providing forward
  declarations for its public types.

- Only include the forwarding header when a forward declaration suffices.

- Implement small functions inline (rule of thumb: 5 lines or less).

Preprocessor Macros
-------------------

- Use macros if and only if you can't get the same result by using inline
  functions, templates, or proper constants.

- Macro names use the form `CAF_<COMPONENT>_<NAME>`.

Comments
--------

- Start Doxygen comments with `///`.

- Use Markdown syntax in Doxygen comments.

- Use `@cmd` rather than `\cmd`.

- Use `//` to define basic comments that should not be processed by Doxygen.

- Never use `/* ... */` C-style comments.
