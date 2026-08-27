---
name: incremental-implementation
description: Delivers changes as thin vertical slices that each leave the system working, tested, and committed. Use when implementing any feature or change that touches more than one file, when you're about to write a large amount of code at once, or when a task feels too big to land in one step.
---

# Incremental Implementation

## Overview

Build in thin vertical slices — implement one piece, test it, verify it, then expand. Avoid implementing an entire feature in one pass. Each increment should leave the system in a working, testable state. This is the execution discipline that makes large features manageable.

Slices derive from tasks; if you don't have a task plan to derive them from, see `planning-and-task-breakdown` first.

## When to Use

- Implementing any multi-file change
- Building a new feature from a task breakdown
- Refactoring existing code
- Any time you're tempted to write more than ~100 lines before testing

**When NOT to use:** Single-file, single-function changes where the scope is already minimal.

## The Increment Cycle

```
┌──────────────────────────────────────┐
│                                      │
│   Implement ──→ Test ──→ Verify ──┐  │
│       ▲                           │  │
│       └───── Commit ◄─────────────┘  │
│              │                       │
│              ▼                       │
│          Next slice                  │
│                                      │
└──────────────────────────────────────┘
```

For each slice:

1. **Implement** the smallest complete piece of functionality
2. **Test** — run the test suite. If following `test-driven-development` (recommended), the failing test that proves this slice's success was already written before step 1; this step confirms it now passes. If not, write a test alongside the implementation.
3. **Verify** — confirm the slice works as expected (tests pass, build succeeds, manual check)
4. **Commit** -- save your progress with a descriptive message (see `git-workflow-and-versioning` for atomic commit guidance)
5. **Move to the next slice** — carry forward, don't restart

## Slicing Strategies

### Vertical Slices (Preferred)

Build one complete path through the stack:

```
Slice 1: Create a task (DB + API + basic UI)
    → Tests pass, user can create a task via the UI

Slice 2: List tasks (query + API + UI)
    → Tests pass, user can see their tasks

Slice 3: Edit a task (update + API + UI)
    → Tests pass, user can modify tasks

Slice 4: Delete a task (delete + API + UI + confirmation)
    → Tests pass, full CRUD complete
```

Each slice delivers working end-to-end functionality.

*This is a web-stack illustration; the same vertical-slice thinking applies to libraries (one public function end-to-end with tests), CLI tools (one command end-to-end), data pipelines (one input-to-output path), embedded firmware (one peripheral working end-to-end), and so on.*

### Contract-First Slicing

When two implementations depend on the same interface — frontend/backend, two services, two libraries against a shared protocol, producer and consumer of a queue — define the contract first, then build both sides against it in parallel:

```
Slice 0: Define the contract (types, interfaces, schema, protocol spec)
Slice 1a: Implement side A against the contract + tests
Slice 1b: Implement side B against the contract using mock data + tests
Slice 2: Integrate and test end-to-end
```

### Risk-First Slicing

Tackle the riskiest or most uncertain piece first:

```
Slice 1: Prove the WebSocket connection works (highest risk)
Slice 2: Build real-time task updates on the proven connection
Slice 3: Add offline support and reconnection
```

If Slice 1 fails, you discover it before investing in Slices 2 and 3.

## Implementation Rules

### Rule 0: Simplicity First

Before writing any code, ask: "What is the simplest thing that could work?"

After writing code, review it against these checks:
- Can this be done in fewer lines?
- Are these abstractions earning their complexity?
- Would a staff engineer look at this and say "why didn't you just..."?
- Am I building for hypothetical future requirements, or the current task?

```
SIMPLICITY CHECK:
✗ Generic EventBus with middleware pipeline for one notification
✓ Simple function call

✗ Abstract factory pattern for two similar components
✓ Two straightforward components with shared utilities

✗ Config-driven form builder for three forms
✓ Three form components
```

Three similar lines of code is better than a premature abstraction. Implement the naive, obviously-correct version first. Optimize only after correctness is proven with tests.

### Rule 0.5: Scope Discipline

Touch only what the task requires.

Do NOT:
- "Clean up" code adjacent to your change
- Refactor imports in files you're not modifying
- Remove comments you don't fully understand
- Add features not in the spec because they "seem useful"
- Modernize syntax in files you're only reading

If you notice something worth improving outside your task scope, note it — don't fix it:

```
NOTICED BUT NOT TOUCHING:
- src/utils/format.ts has an unused import (unrelated to this task)
- The auth middleware could use better error messages (separate task)
→ Want me to create tasks for these?
```

### Rule 1: One Thing at a Time

Each increment changes one logical thing. Don't mix concerns:

**Bad:** One commit that adds a new component, refactors an existing one, and updates the build config.

**Good:** Three separate commits — one for each change.

### Rule 2: Keep It Compilable

After each increment, the project must build and existing tests must pass. Don't leave the codebase in a broken state between slices.

**You don't get to dismiss a failure.** When a test fails or the build breaks during your work, you have exactly three options:

1. **Fix it now**, as a *separate commit* from your current slice. Yes, this pollutes scope — accept it. A 20-line bug fix folded into a feature slice (as its own commit) is far less harmful than a real bug shipping behind a "probably unrelated" rationalization.
2. **Stop and file it.** Park the slice, open a task or issue for the failure, resume your work once it's handled (by you or someone else).
3. **Prove it's pre-existing — but only if proving it is cheap.** A fast unit test you can re-run on HEAD, a trivial reproduce on the base branch — do it. A 10-minute integration suite — skip straight to option 1 or 2. The check must be cheaper than just fixing the bug; otherwise you're rationalizing via effort.

**What you may not do:** dismiss the failure with *"probably flaky,"* *"probably unrelated,"* or *"probably was already there."* The self-absolving narrative is the actual failure mode this rule exists to block — same family as debugging's *"I'll just try changing X and see."* If you haven't done one of the three things above, you haven't earned the right to move on.

### Rule 3: Feature Flags for Incomplete Features

If a feature isn't ready for users but you need to merge increments, gate the new behavior behind a flag — environment variable, config setting, runtime check, build-time constant, whatever fits the project. The default state is *off*.

```
if feature_flag_x is enabled:
    [new behavior]
else:
    [existing behavior]
```

This lets you merge small increments to the main branch without exposing incomplete work to users.

### Rule 4: Safe Defaults

New code should default to safe, conservative behavior. Optional behaviors with side effects — sending notifications, retrying, caching, parallelism, eager loading — should be **opt-in, not opt-out**. The default should be what a cautious caller would pick if they didn't know the implications.

```
function createTask(data, options):
    shouldNotify = options.notify if provided else false   # opt-in, not opt-out
    ...
```

### Rule 5: Rollback-Friendly

Each increment should be independently revertable:

- Additive changes (new files, new functions) are easy to revert
- Modifications to existing code should be minimal and focused
- Database migrations should have corresponding rollback migrations
- Avoid deleting something in one commit and replacing it in the same commit — separate them

## Increment Checklist

After each increment, verify:

- [ ] The change does one thing and does it completely
- [ ] All existing tests still pass
- [ ] The build succeeds
- [ ] Type checking passes (where applicable)
- [ ] Linting passes
- [ ] The new functionality works as expected
- [ ] The change is committed with a descriptive message

## Common Rationalizations

| Rationalization | Reality |
|---|---|
| "I'll test it all at the end" | Bugs compound. A bug in Slice 1 makes Slices 2-5 wrong. Test each slice. |
| "It's faster to do it all at once" | It *feels* faster until something breaks and you can't find which of 500 changed lines caused it. |
| "These changes are too small to commit separately" | Small commits are free. Large commits hide bugs and make rollbacks painful. |
| "I'll add the feature flag later" | If the feature isn't complete, it shouldn't be user-visible. Add the flag now. |
| "This refactor is small enough to include" | Refactors mixed with features make both harder to review and debug. Separate them. |
| "That test was already failing" / "It's probably flaky" / "It looks unrelated" | Maybe. Did you prove it? If proving it costs more than fixing it, just fix it (separate commit) or file it. You don't get to dismiss it. |

## Red Flags

- More than 100 lines of code written without running tests
- Multiple unrelated changes in a single increment
- "Let me just quickly add this too" scope expansion
- Skipping the test/verify step to move faster
- Build or tests broken between increments
- Large uncommitted changes accumulating
- Building abstractions before the third use case demands it
- Touching files outside the task scope "while I'm here"
- Creating new utility files for one-time operations
- Dismissing a test failure or build break with "probably flaky," "probably unrelated," or "probably was already there" without either (a) proving it cheaply, (b) fixing it as a separate commit, or (c) filing it and parking the slice

## Verification

After completing all increments for a task:

- [ ] Each increment was individually tested and committed
- [ ] The full test suite passes
- [ ] The build is clean
- [ ] The feature works end-to-end as specified
- [ ] No uncommitted changes remain
