---
name: test-driven-development
description: Drives development with tests — write failing tests first, then code that makes them pass; for bugs, write the reproduction test before attempting a fix. Use when implementing any logic, fixing any bug, modifying any behavior, or proving that code works.
---

# Test-Driven Development

## Overview

Write a failing test before writing the code that makes it pass. For bug fixes, reproduce the bug with a test before attempting a fix. Tests are proof — *"seems right"* is not done. A codebase with good tests is an AI agent's superpower; a codebase without tests is a liability.

## When to Use

- Implementing any new logic or behavior
- Fixing any bug (the Prove-It Pattern)
- Modifying existing functionality
- Adding edge case handling
- Any change that could break existing behavior

**When NOT to use:** Pure configuration changes, documentation updates, or static content changes that have no behavioral impact.

**Where this fits:** TDD is the test side of `incremental-implementation`. Each slice's failing test is written here (RED) before the slice's implement step. Acceptance criteria from `planning-and-task-breakdown` arrive already shaped to be test-able. The Prove-It Pattern below IS the *Guard Against Recurrence* step from `debugging-and-error-recovery`, applied at bug-discovery time rather than at the end of debugging.

## The TDD Cycle

```
    RED                GREEN              REFACTOR
 Write a test    Write minimal code    Clean up the
 that fails  ──→  to make it pass  ──→  implementation  ──→  (repeat)
      │                  │                    │
      ▼                  ▼                    ▼
   Test FAILS        Test PASSES         Tests still PASS
```

### Step 1: RED — Write a Failing Test

Write the test first. It must fail. A test that passes immediately proves nothing — either the behavior already exists (no work to do) or your test isn't actually testing what you think.

```
test "create_task sets default status and timestamp":
    task = create_task(title="Buy groceries")
    assert task.id is defined
    assert task.title == "Buy groceries"
    assert task.status == "pending"
    assert task.created_at is a timestamp
```

### Step 2: GREEN — Make It Pass

Write the minimum code to make the test pass. Don't over-engineer. Naive-and-correct beats elegant-and-incomplete.

### Step 3: REFACTOR — Clean Up

With tests green, improve the code without changing behavior:

- Extract shared logic
- Improve naming
- Remove duplication
- Optimize if measurement says you need to

Run tests after every refactor step to confirm nothing broke.

## The Prove-It Pattern (Bug Fixes)

When a bug is reported, **do not start by trying to fix it.** Start by writing a test that reproduces it.

```
Bug report arrives
       │
       ▼
  Write a test that demonstrates the bug
       │
       ▼
  Test FAILS (confirming the bug exists)
       │
       ▼
  Implement the fix
       │
       ▼
  Test PASSES (proving the fix works)
       │
       ▼
  Run full test suite (no regressions)
```

**Example arc:**

```
Bug: "Completing a task doesn't update the completed_at timestamp"

Step 1: Write the reproduction test (it should FAIL)
    test "completing a task sets completed_at":
        task = create_task(title="Test")
        completed = complete_task(task.id)
        assert completed.status == "completed"
        assert completed.completed_at is a timestamp   # ← fails → bug confirmed

Step 2: Implement the fix (e.g., the update statement was missing the field)

Step 3: Test passes → bug fixed AND regression guarded against forever
```

This pattern is the *Guard Against Recurrence* step from `debugging-and-error-recovery`, applied as soon as the bug is discovered rather than at the end. The test is the artifact that prevents the bug from coming back the next time someone touches the area.

## The Test Pyramid

Invest testing effort according to the pyramid — most tests should be small and fast, with progressively fewer tests at higher levels:

```
          ╱╲
         ╱  ╲         E2E Tests (~5%)
        ╱    ╲        Full user flows, real browser
       ╱──────╲
      ╱        ╲      Integration Tests (~15%)
     ╱          ╲     Component interactions, API boundaries
    ╱────────────╲
   ╱              ╲   Unit Tests (~80%)
  ╱                ╲  Pure logic, isolated, milliseconds each
 ╱──────────────────╲
```

**The Beyonce Rule:** If you liked it, you should have put a test on it. Infrastructure changes, refactoring, and migrations are not responsible for catching your bugs — your tests are. If a change breaks your code and you didn't have a test for it, that's on you.

### Test Sizes (Resource Model)

Beyond the pyramid levels, classify tests by what resources they consume:

| Size | Constraints | Speed | Example |
|------|------------|-------|---------|
| **Small** | Single process, no I/O, no network, no database | Milliseconds | Pure function tests, data transforms |
| **Medium** | Multi-process OK, localhost only, no external services | Seconds | API tests with test DB, component tests |
| **Large** | Multi-machine OK, external services allowed | Minutes | E2E tests, performance benchmarks, staging integration |

Small tests should make up the vast majority of your suite. They're fast, reliable, and easy to debug when they fail.

### Decision Guide

```
Is it pure logic with no side effects?
  → Unit test (small)

Does it cross a boundary (API, database, file system)?
  → Integration test (medium)

Is it a critical user flow that must work end-to-end?
  → E2E test (large) — limit these to critical paths
```

## Writing Good Tests

### Test State, Not Interactions

Assert on the *outcome* of an operation, not on which methods were called internally. Tests that verify method-call sequences break when you refactor, even if the behavior is unchanged.

```
# Good — tests what the function does (state-based)
test "list_tasks returns tasks sorted by creation date, newest first":
    tasks = list_tasks(sort_by="created_at", sort_order="desc")
    assert tasks[0].created_at > tasks[1].created_at

# Bad — tests how the function works internally (interaction-based)
test "list_tasks calls db with ORDER BY":
    list_tasks(sort_by="created_at", sort_order="desc")
    assert db.query was called with a string containing "ORDER BY created_at DESC"
```

The "bad" version breaks the moment you swap ORMs, rewrite the query, or move to a different storage layer — even though the behavior is identical. The "good" version survives all of those.

### DAMP Over DRY in Tests

In production code, DRY (Don't Repeat Yourself) is usually right. In tests, **DAMP (Descriptive And Meaningful Phrases)** is better. A test should read like a specification — each test should tell a complete story without requiring the reader to trace through shared helpers.

```
# DAMP — each test is self-contained and readable
test "rejects tasks with empty titles":
    input = { title: "", assignee: "user-1" }
    assert create_task(input) raises "Title is required"

test "trims whitespace from titles":
    input = { title: "  Buy groceries  ", assignee: "user-1" }
    task = create_task(input)
    assert task.title == "Buy groceries"
```

Duplication in tests is acceptable when it makes each test independently understandable. Hiding the input shape behind a `make_default_input()` helper is fine for one parameter; it's harmful when reading the test requires you to also read the helper.

### Prefer Real Implementations Over Mocks

Use the simplest test double that gets the job done. The more your tests use real code, the more confidence they provide.

```
Preference order (most to least preferred):
1. Real implementation  → Highest confidence, catches real bugs
2. Fake                 → In-memory version of a dependency (e.g., fake DB)
3. Stub                 → Returns canned data, no behavior
4. Mock (interaction)   → Verifies method calls — use sparingly
```

**Use mocks only when:** the real implementation is too slow, non-deterministic, or has side effects you can't control (external APIs, email sending, payment processing). Over-mocking creates tests that pass while production breaks.

### Use the Arrange-Act-Assert Pattern

Structure each test in three visually distinct sections:

```
test "marks overdue tasks when deadline has passed":
    # Arrange — set up the scenario
    task = create_task(title="Test", deadline="2025-01-01")

    # Act — perform the action being tested
    result = check_overdue(task, now="2025-01-02")

    # Assert — verify the outcome
    assert result.is_overdue == true
```

A reader should see at a glance what's setup, what's the operation under test, and what's the verification. Blank lines, comments, or both — whichever fits the language.

### One Assertion Per Concept

```
# Good — each test verifies one behavior
test "rejects empty titles": ...
test "trims whitespace from titles": ...
test "enforces maximum title length": ...

# Bad — everything in one test, can't tell which assertion failed without re-running
test "validates titles correctly":
    assert create_task({ title: "" }) raises ...
    assert create_task({ title: "  hello  " }).title == "hello"
    assert create_task({ title: very_long_string }) raises ...
```

"One assertion per concept" doesn't mean literally one `assert` per test — it means each test verifies one logical behavior. Multiple assertions about the same outcome (`task.id`, `task.title`, `task.status` after a single create call) are one concept and belong in one test.

### Name Tests Descriptively

```
# Good — reads like a specification
TaskService.complete_task:
    sets status to completed and records timestamp
    throws NotFoundError for non-existent task
    is idempotent — completing an already-completed task is a no-op
    sends notification to task assignee

# Bad — vague names
TaskService:
    works
    handles errors
    test 3
```

Good test names double as documentation. A reader should be able to scan the test list and understand the contract of the code under test without reading any test bodies.

## Test Anti-Patterns to Avoid

| Anti-Pattern | Problem | Fix |
|---|---|---|
| Testing implementation details | Tests break when refactoring even if behavior is unchanged | Test inputs and outputs, not internal structure |
| Flaky tests (timing, order-dependent) | Erode trust in the test suite | Use deterministic assertions, isolate test state |
| Testing framework code | Wastes time testing third-party behavior | Only test YOUR code |
| Snapshot abuse | Large snapshots nobody reviews, break on any change | Use snapshots sparingly and review every change |
| No test isolation | Tests pass individually but fail together | Each test sets up and tears down its own state |
| Mocking everything | Tests pass but production breaks | Prefer real implementations > fakes > stubs > mocks. Mock only at boundaries where real deps are slow or non-deterministic |

## When to Use Subagents for Testing

For complex bug fixes, fan out the bug-reproduction test to a subagent. The dispatcher writes the brief without disclosing the suspected fix; the subagent writes the test from the bug description alone, which makes the test target the *behavior* rather than the suspected mechanism.

```
Main agent: "Spawn a subagent with the bug description and the
relevant area of the codebase. Brief: write a test that
demonstrates the bug. The test should fail with the current code.
Don't propose a fix."

Subagent: Returns the failing reproduction test.

Main agent: Verifies it fails, implements the fix, verifies it passes.
```

This is a fan-out brief in the sense of `planning-and-task-breakdown` — the subagent operates from a self-contained brief without the dispatcher's hypothesis about the cause. Result: the test verifies that the bug is fixed, not that the dispatcher's theory was right.

## Common Rationalizations

| Rationalization | Reality |
|---|---|
| "I'll write tests after the code works" | You won't. And tests written after the fact test implementation, not behavior. |
| "This is too simple to test" | Simple code gets complicated. The test documents the expected behavior. |
| "Tests slow me down" | Tests slow you down now. They speed you up every time you change the code later. |
| "I tested it manually" | Manual testing doesn't persist. Tomorrow's change might break it with no way to know. |
| "The code is self-explanatory" | Tests ARE the specification. They document what the code should do, not what it does. |
| "It's just a prototype" | Prototypes become production code. Tests from day one prevent the "test debt" crisis. |

## Red Flags

- Writing code without any corresponding tests
- Tests that pass on the first run (they may not be testing what you think)
- "All tests pass" but no tests were actually run
- Bug fixes without reproduction tests
- Tests that test framework behavior instead of application behavior
- Test names that don't describe the expected behavior
- Skipping tests to make the suite pass

## Verification

After completing any implementation:

- [ ] Every new behavior has a corresponding test
- [ ] All tests pass
- [ ] Bug fixes include a reproduction test that failed before the fix
- [ ] Test names describe the behavior being verified
- [ ] No tests were skipped or disabled
- [ ] Coverage hasn't decreased (if tracked)
