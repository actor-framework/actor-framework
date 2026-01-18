# Code Review Task

You are performing a critical code review on a PR branch.

## Instructions

1. First, get the diff of all changes on this branch compared to main:
   ```bash
   git diff $(git merge-base main HEAD)
   ```

2. Carefully and critically review all changes in the diff.

3. Focus your review on finding:
   - **Potential bugs** - Logic errors, off-by-one errors, null/undefined issues, race conditions, resource leaks, etc.
   - **Security vulnerabilities** - Injection risks, improper input validation, authentication/authorization issues, sensitive data exposure, etc.
   - **Best-practice/style violations** - Inconsistent naming, poor error handling, missing edge cases, code smells, violations of project conventions, etc.

4. Do NOT provide:
   - Generic praise or "this looks good" feedback
   - Summaries of what the PR does (assume the author knows)
   - Trivial nitpicks unless they indicate a pattern

5. For each issue found, provide:
   - The specific file and location
   - What the problem is
   - Why it's a problem
   - A suggested fix (if applicable)

6. If you find no significant issues, say so briefly rather than inventing concerns.

Be thorough, skeptical, and constructive. Treat this as if you're reviewing code that will run in production.
