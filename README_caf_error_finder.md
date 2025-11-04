# Finding Functions That Return `caf::error`

This directory contains tools to find all function definitions that return `caf::error` in the C++ Actor Framework (CAF) codebase.

## Files

1. **`find_caf_error_functions.query`** - clang-query AST matcher queries
2. **`run_caf_error_query.sh`** - Script to run clang-query (requires clang-tools)  
3. **`find_caf_error_functions_grep.sh`** - Alternative grep-based approach
4. **`README_caf_error_finder.md`** - This documentation

## Method 1: clang-query (Recommended)

The clang-query approach is the most accurate as it uses the C++ AST to find function definitions.

### Prerequisites

Install clang-tools:
```bash
# Ubuntu/Debian
sudo apt install clang-tools

# macOS with Homebrew  
brew install llvm

# Fedora/CentOS
sudo dnf install clang-tools-extra
```

### Usage

```bash
# Run on all source files (limited to first 20 for performance)
./run_caf_error_query.sh

# Run on specific files
./run_caf_error_query.sh libcaf_core/caf/error.cpp libcaf_net/caf/net/socket_manager.cpp

# Run with custom parameters
clang-query file.cpp -f find_caf_error_functions.query -- -I libcaf_core/caf -std=c++17
```

### Query Details

The query file contains four different AST matchers:

1. **Basic Query**: Matches functions returning exactly `caf::error`
   ```cpp
   match functionDecl(
     isDefinition(),
     hasReturnType(qualType(asString("caf::error")))
   )
   ```

2. **Comprehensive Query**: Matches const and reference variations
   ```cpp
   match functionDecl(
     isDefinition(), 
     hasReturnType(qualType(anyOf(
       asString("caf::error"),
       asString("const caf::error"),
       asString("caf::error &"),
       asString("const caf::error &"),
       asString("caf::error&&")
     )))
   )
   ```

3. **Type-based Query**: Matches by declaration name (broader match)
   ```cpp
   match functionDecl(
     isDefinition(),
     hasReturnType(qualType(hasDeclaration(namedDecl(hasName("error")))))
   )
   ```

4. **Namespace-specific Query**: Matches `error` specifically in `caf` namespace
   ```cpp
   match functionDecl(
     isDefinition(),
     hasReturnType(qualType(hasDeclaration(
       cxxRecordDecl(
         hasName("error"),
         hasParent(namespaceDecl(hasName("caf")))
       )
     )))
   )
   ```

## Method 2: grep-based (Fallback)

When clang-query is not available, use the grep-based approach:

```bash
./find_caf_error_functions_grep.sh
```

This script uses regular expressions to find:
- Functions with explicit `caf::error` return type
- Functions returning `error` (likely in caf namespace)  
- Method definitions returning `error`
- Lambda expressions returning `error`

### Limitations of grep approach

- May include false positives (comments, non-function contexts)
- Cannot distinguish between function declarations vs definitions
- Less precise type matching
- May miss complex return type patterns

## Example Output

From the CAF codebase, typical functions found include:

```cpp
// Network layer functions
error start(net::socket_manager* owner) override;
error accept(const net::http::request_header& hdr) override;

// Configuration parsing  
error parse(std::vector<std::string> args);
error parse_config(std::istream& source, const config_option_set& opts);

// Async operations
error abort_reason() const;
const error& fail_reason() const;

// Type parsing
error parse(std::string_view str, ipv4_address& dest);
error parse(std::string_view str, uuid& dest);
```

## Integration with Development Workflow

You can integrate these queries into:

1. **Code Reviews**: Check for consistent error handling patterns
2. **Refactoring**: Find all error-returning functions when changing error types
3. **Documentation**: Generate lists of error-producing functions  
4. **Testing**: Ensure all error paths are covered

## Performance Notes

- clang-query can be slow on large codebases
- The script limits to 20 files by default for initial testing
- For full codebase analysis, consider running on specific directories
- Use parallel processing for large-scale analysis

## Extending the Queries

To modify the queries for other return types:

1. Replace `"error"` with your target type name
2. Replace `"caf"` with your target namespace  
3. Add additional `asString()` patterns for type variations
4. Consider template instantiations using `hasDeclaration()`