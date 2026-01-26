# Troubleshoot Build and Tests

You are troubleshooting build and test issues for this project.

## Instructions

### Phase 1: Build the Project

1. Build the project.

2. If the build fails:
   - Fix any compilation errors.
   - After each fix, rebuild the project to verify the fix works.
   - Continue until the build succeeds.

3. Once the build succeeds, proceed to Phase 2.

### Phase 2: Run Unit Tests

1. Run the full unit test suite.

2. If tests fail:
   - Identify which tests are failing.
   - Debug the failing tests. Focus on one test at a time.
   - After fixing a test, run the full test suite again to ensure that the fix
     did not break other tests.

3. Continue debugging and fixing until all tests pass.

### General Guidelines

- Be systematic: Fix one issue at a time and verify the fix before moving on
- Check the project's coding conventions in `CONTRIBUTING.md` when making fixes
- Ensure fixes don't break existing functionality
- Document any non-obvious fixes or workarounds if needed

Your goal is to get a clean build and a fully passing test suite.
