---
name: planning-and-task-breakdown
description: Breaks work into small, ordered, verifiable tasks with explicit acceptance criteria. Use when you have a spec or clear requirements that need decomposition, when a task feels too large to start, when you need to estimate scope, or when planning fan-out work for multiple agents.
---

# Planning and Task Breakdown

## Overview

Decompose work into small, verifiable tasks with explicit acceptance criteria. Good task breakdown is the difference between an agent that completes work reliably and one that produces a tangled mess. Every task should be small enough to implement, test, and verify in a single focused session.

## When to Use

- You have a spec and need to break it into implementable units
- A task feels too large or vague to start
- Work needs to be parallelized across multiple agents or sessions
- You need to communicate scope to a human
- The implementation order isn't obvious

**When NOT to use:** Single-file changes with obvious scope, or when the spec already contains well-defined tasks.

## The Planning Process

### Step 1: Plan Before Coding

Before writing any code, do read-only work:

- **Read the spec first** (if one exists). The plan must derive from the spec — if no spec exists or it's unclear, stop and produce a spec before planning.
- Read the relevant codebase sections to understand existing patterns and conventions
- Map dependencies between components
- Note risks and unknowns

**Do NOT write code during planning.** The output is a plan document, not implementation.

### Step 2: Identify the Dependency Graph

Map what depends on what:

```
Database schema
    │
    ├── API models/types
    │       │
    │       ├── API endpoints
    │       │       │
    │       │       └── Frontend API client
    │       │               │
    │       │               └── UI components
    │       │
    │       └── Validation logic
    │
    └── Seed data / migrations
```

*This is a typical web-stack example; the same dependency-graph thinking applies to libraries, CLI tools, data pipelines, embedded firmware — anywhere one layer's interface defines another's input.*

Implementation order follows the dependency graph bottom-up: build foundations first.

### Step 3: Slice Vertically

Instead of building all the database, then all the API, then all the UI — build one complete feature path at a time:

**Bad (horizontal slicing):**
```
Task 1: Build entire database schema
Task 2: Build all API endpoints
Task 3: Build all UI components
Task 4: Connect everything
```

**Good (vertical slicing):**
```
Task 1: User can create an account (schema + API + UI for registration)
Task 2: User can log in (auth schema + API + UI for login)
Task 3: User can create a task (task schema + API + UI for creation)
Task 4: User can view task list (query + API + UI for list view)
```

Each vertical slice delivers working, testable functionality.

### Step 4: Write Tasks

Each task follows this structure:

```markdown
## Task [N]: [Short descriptive title]

**Description:** One paragraph explaining what this task accomplishes.

**Acceptance criteria:**
- [ ] [Specific, testable condition]
- [ ] [Specific, testable condition]

Acceptance criteria must be specific enough that the implementer can write a failing test against them before writing code (see `test-driven-development` for the mechanics).

**Verification:**
- [ ] Relevant tests pass
- [ ] Build succeeds
- [ ] Manual check: [description of what to verify]

**Dependencies:** [Task numbers this depends on, or "None"]

**Files likely touched:**
- `src/path/to/file`
- `tests/path/to/test`

**Estimated scope:** [Small: 1-2 files | Medium: 3-5 files | Large: 5+ files]
```

### Step 5: Order and Checkpoint

Arrange tasks so that:

1. Dependencies are satisfied (build foundation first)
2. Each task leaves the system in a working state
3. Verification checkpoints occur after every 2-3 tasks
4. High-risk tasks are early (fail fast)

Add explicit checkpoints:

```markdown
## Checkpoint: After Tasks 1-3
- [ ] All tests pass
- [ ] Application builds without errors
- [ ] Core user flow works end-to-end
- [ ] Review with human before proceeding
```

## Task Sizing Guidelines

| Size | Files | Scope | Example |
|------|-------|-------|---------|
| **XS** | 1 | Single function or config change | Add a validation rule |
| **S** | 1-2 | One component or endpoint | Add a new API endpoint |
| **M** | 3-5 | One feature slice | User registration flow |
| **L** | 5-8 | Multi-component feature | Search with filtering and pagination |
| **XL** | 8+ | **Too large — break it down further** | — |

If a task is L or larger, it should be broken into smaller tasks. An agent performs best on S and M tasks.

**When to break a task down further:**
- It would take more than one focused session (roughly 2+ hours of agent work)
- You cannot describe the acceptance criteria in 3 or fewer bullet points
- It touches two or more independent subsystems (e.g., auth and billing)
- You find yourself writing "and" in the task title (a sign it is two tasks)
- You cannot imagine the failing test that would prove this task done

## Plan Document Template

```markdown
# Implementation Plan: [Feature/Project Name]

## Overview
[One paragraph summary of what we're building]

## Architecture Decisions
- [Key decision 1 and rationale]
- [Key decision 2 and rationale]

## Task List

### Phase 1: Foundation
- [ ] Task 1: ...
- [ ] Task 2: ...

### Checkpoint: Foundation
- [ ] Tests pass, builds clean

### Phase 2: Core Features
- [ ] Task 3: ...
- [ ] Task 4: ...

### Checkpoint: Core Features
- [ ] End-to-end flow works

### Phase 3: Polish
- [ ] Task 5: ...
- [ ] Task 6: ...

### Checkpoint: Complete
- [ ] All acceptance criteria met
- [ ] Ready for review

## Risks and Mitigations
| Risk | Impact | Mitigation |
|------|--------|------------|
| [Risk] | [High/Med/Low] | [Strategy] |

## Open Questions
- [Question needing human input]
```

**Before saving the plan, ask the user where and in what format.** Don't assume a default location. Many projects have conventions — `docs/plans/`, `planning/`, a GitHub issue, a Linear or Jira ticket, a checked-in markdown file in the feature branch, etc. Ask, and follow theirs.

## Identifying Work for Multiple Agents

Modern agent workflows let you fan out independent work to subagents while the main thread coordinates. Planning is where you decide what fans out and what stays — not at execution time, when context is already loaded with implementation details.

**What's safe to fan out:**
- Independent vertical slices that don't share files
- Bounded research and exploration tasks (return findings, not changes)
- Test-writing for already-completed code
- Documentation generation
- Log analysis, artifact inspection, codebase surveys

**What must stay in the main thread:**
- Tasks touching shared state (database migrations, shared config, public APIs the whole plan depends on)
- Tasks where the contract isn't defined yet
- Anything requiring deep planning context to make tradeoff decisions
- Anything where the agent needs to negotiate with the user mid-task

**What needs coordination before fan-out:**
- Features that share an interface — define the contract in the main thread first, then fan out the implementations
- Tasks that converge on the same files — sequence them, don't parallelize
- Add explicit merge/integration checkpoints in the plan wherever parallel work converges

**A fan-out task needs a self-contained brief.** The subagent has none of the planning context — it sees only the brief. The brief must include: the task's goal, the context the subagent doesn't already have, what success looks like, what files are in scope, and what NOT to touch. If you can't write that brief during planning, the task isn't ready to fan out.

## Common Rationalizations

| Rationalization | Reality |
|---|---|
| "I'll figure it out as I go" | That's how you end up with a tangled mess and rework. 10 minutes of planning saves hours. |
| "The tasks are obvious" | Write them down anyway. Explicit tasks surface hidden dependencies and forgotten edge cases. |
| "Planning is overhead" | Planning is the task. Implementation without a plan is just typing. |
| "I can hold it all in my head" | Context windows are finite. Written plans survive session boundaries and compaction. |

## Red Flags

- Starting implementation without a written task list
- Tasks that say "implement the feature" without acceptance criteria
- No verification steps in the plan
- All tasks are XL-sized
- No checkpoints between tasks
- Dependency order isn't considered

## Verification

Before starting implementation, confirm:

- [ ] Every task has acceptance criteria
- [ ] Each task's acceptance criteria can be expressed as a failing test
- [ ] Every task has a verification step
- [ ] Task dependencies are identified and ordered correctly
- [ ] No task touches more than ~5 files
- [ ] Checkpoints exist between major phases
- [ ] Any task identified for fan-out has a brief a subagent could execute cold
- [ ] The human has reviewed and approved the plan
