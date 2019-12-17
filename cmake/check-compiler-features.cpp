#ifndef __cpp_noexcept_function_type
#  error "Noexcept not part of the type system (__cpp_noexcept_function_type)"
#endif

#ifndef __cpp_fold_expressions
#  error "No support for fold expression (__cpp_fold_expressions)"
#endif

#ifndef __cpp_if_constexpr
#  error "No support for 'if constexpr' (__cpp_if_constexpr)"
#endif

// Unfortunately there's no feature test macro for thread_local. By putting this
// here, at least we'll get a compiler error on unsupported platforms.
[[maybe_unused]] thread_local int foo;

int main(int, char**) {
  return 0;
}
