# PR 2154 Summary: Reduce Complexity of the Response Promises

## Overview

**PR Title:** Reduce complexity of the response promises  
**PR Number:** 2154  
**Status:** Merged  
**Author:** Neverlord  
**Merged Date:** October 27, 2025  
**Related Issues:** #2009, #1873

## Summary

This pull request was a major refactoring effort aimed at simplifying and improving the response promise implementation in the actor-framework. The PR focused on three main objectives:

1. **Redesign `as_single` and `as_observable` on requests** - Improved the API for converting request responses to flow observables
2. **Use a consistent setup for response handles** - Standardized the implementation across different response handle types
3. **Remove the `requester` mixin** - Deprecated the old `requester` mixin in favor of the new mail API

## Key Changes

### Files Changed
- **56 files changed** with **755 additions** and **2,207 deletions** (net reduction of 1,452 lines)

### Major Modifications

1. **Response Handle Refactoring**
   - Simplified `event_based_response_handle.hpp` and `event_based_fan_out_response_handle.hpp`
   - Introduced consistent state structures for response handles
   - Improved `as_single()` and `as_observable()` implementations with better template metaprogramming

2. **Mixin Requester Deprecation**
   - Converted the `mixin::requester` class into a macro (`CAF_ADD_DEPRECATED_REQUEST_API`)
   - Added deprecation warnings for the old `request()` and `fan_out_request()` APIs
   - Removed `mixin/requester.test.cpp` (508 lines removed)

3. **Actor Base Classes**
   - Updated `event_based_actor` and `blocking_actor` to remove dependency on the requester mixin
   - Simplified inheritance hierarchies

4. **Flow API Improvements**
   - Enhanced `flow/op/cell.hpp`, `flow/op/mcast.hpp`, and `flow/op/ucast.hpp`
   - Renamed `push_all()` to `push()` in multicast operators (with deprecation for backward compatibility)
   - Added `single_value` constants to flow operators

5. **Metaprogramming Utilities**
   - Created new `caf/detail/metaprogramming.hpp` header
   - Moved utility traits like `unboxed`, `always_false`, `left`, and `implicit_cast` to centralized location
   - Cleaned up `caf/detail/concepts.hpp` by removing redundant definitions

6. **Test Updates**
   - Updated `event_based_mail.test.cpp` to reflect new observable semantics
   - Removed obsolete test files: `mixin/requester.test.cpp`, `response_handle.test.cpp`, `policy/select_all.test.cpp`, `policy/select_any.test.cpp`

### Breaking Changes

The PR introduced behavioral changes in how fan-out requests work with observables:
- Previously, `as_observable()` on `select_all` returned `std::vector<T>` elements
- Now it returns individual `T` elements (one per response)
- This is more intuitive and aligns better with reactive programming patterns

## TODO Items Left in the Code

As mentioned in the PR description, there are **two TODO items** that were intentionally left for follow-up work:

### 1. TODO in `libcaf_core/caf/typed_response_promise.test.cpp` (Line 130)

```cpp
// TODO: https://github.com/actor-framework/actor-framework/issues/2009
// WHEN("sending ok_atom to the dispatcher synchronously") {
//   auto res = self->request(hdl, infinite, ok_atom_v);
//   auto fetch_result = [&] {
//     message result;
//     res.receive([] {}, // void result
```

**Context:** This test case is commented out and needs to be re-enabled after further work on issue #2009.

### 2. TODO in `libcaf_core/caf/response_promise.test.cpp` (Line 125)

```cpp
// TODO: https://github.com/actor-framework/actor-framework/issues/2009
// WHEN("sending ok_atom to the dispatcher synchronously") {
//   auto res = self->request(hdl, infinite, ok_atom_v);
//   auto fetch_result = [&] {
//     message result;
//     res.receive([] {}, // void result
```

**Context:** Similar to the above, this test case is commented out and related to blocking promise functionality.

### Additional TODO Items Found in the Codebase

While searching the entire codebase, here are other notable TODOs (not necessarily related to PR 2154):

1. **`libcaf_io/caf/io/basp_broker.cpp:71`** - Blocking thread concern
2. **`libcaf_net/caf/net/http/server.cpp:190, 213`** - HTTP payload and implementation TODOs
3. **`libcaf_test/caf/test/fixture/flow.cpp:25`** - Scoped coordinator improvement needed
4. **`libcaf_core/caf/actor_system.cpp:936, 944`** - Module compatibility issues
5. **`libcaf_core/caf/is_error_code_enum.hpp:7`** - Scheduled for removal in CAF 3.0.0
6. **`libcaf_core/caf/binary_serializer.cpp:269`** - IEEE-754 long double conversion
7. **`libcaf_core/caf/typed_behavior.hpp:186`** - Type checking implementation needed

## Review Discussion Highlights

From the PR review comments:

1. **Interface improvements** (riemass): The new observable API is cleaner, eliminating the need to work with vectors when using `select_all`.

2. **Type safety** (riemass): Questions about the templated versions and type deduction with the new API - resolved that C++'s template instantiation allows for type obfuscation until actual use.

3. **Breaking changes** (riemass): Confirmed that users can use `to_vector()` on observables if they need to collect all values.

4. **Follow-up work** (riemass): The blocking fan-out request implementation was mentioned as follow-up work to be done in PR #2132.

## Impact and Benefits

1. **Code Reduction:** Removed over 1,400 lines of code while improving functionality
2. **Simplified API:** More intuitive flow-based response handling
3. **Better Separation of Concerns:** Clearer distinction between response handles and flow operations
4. **Improved Maintainability:** Centralized metaprogramming utilities
5. **Future-Ready:** Deprecation path for old API while maintaining backward compatibility

## Next Steps

According to the PR discussion:
- The two TODO items should be addressed when rebasing and fixing PR #2132
- The blocking promises implementation was deliberately not touched in this PR to avoid conflicts
- Further work on issue #2009 is needed to complete the simplification effort
