---
name: spec-driven-development
description: Creates a structured specification before any code. Use when starting a new project, feature, or significant change without an existing spec, or when requirements are unclear, ambiguous, or only exist as a vague idea.
---

# Spec-Driven Development

## Overview

Write a structured specification before writing any code. The spec is the shared source of truth between you and the human engineer — it defines what we're building, why, and how we'll know it's done. Code without a spec is guessing.

## When to Use

- Starting a new project or feature
- Requirements are ambiguous or incomplete
- The change touches multiple files or modules
- You're about to make an architectural decision
- The task would take more than 30 minutes to implement

**When NOT to use:** Single-line fixes, typo corrections, or changes where requirements are unambiguous and self-contained.

## The Gated Workflow

Spec-driven development has four phases. Do not advance to the next phase until the current one is validated.

```
SPECIFY ──→ PLAN ──→ TASKS ──→ IMPLEMENT
   │          │        │          │
   ▼          ▼        ▼          ▼
 Human      Human    Human      Human
 reviews    reviews  reviews    reviews
```

### Phase 1: Specify

Start with a high-level vision. Ask the human clarifying questions until requirements are concrete.

**Surface assumptions immediately.** Before writing any spec content, list what you're assuming:

```
ASSUMPTIONS I'M MAKING:
1. This is a web application (not native mobile)
2. Authentication uses session-based cookies (not JWT)
3. The database is PostgreSQL (based on existing Prisma schema)
4. We're targeting modern browsers only (no IE11)
→ Correct me now or I'll proceed with these.
```

Don't silently fill in ambiguous requirements. The spec's entire purpose is to surface misunderstandings *before* code gets written — assumptions are the most dangerous form of misunderstanding.

**Distinguish requirements from implementation.** A spec captures the problem, the user, success criteria, and constraints — *not* which libraries, frameworks, data structures, or algorithms to use. *"Use React Query for data fetching"* is an implementation choice that belongs in Phase 2 (Plan), not in the spec. The spec should be technology-neutral enough that a different engineer could read it and propose a different implementation.

*Exception:* technology constraints that are genuinely *given* — *"must run on the existing Postgres database,"* *"extend the current Django project,"* *"browser-only, no backend changes"* — belong in the spec because they're constraints, not choices. The test: if the choice was made *for* you and isn't open for debate, it's a constraint; if you're picking it because you think it's the best tool for the job, it's a Phase 2 decision.

**Anchor in existing code.** If the spec is for work inside an existing codebase (most cases), read the codebase before writing the spec. Reference specific files, modules, and patterns the new work will touch or extend. A spec that ignores existing architecture produces implementation work that fights the codebase. The spec should make obvious what's reused, what's extended, and what's net-new.

**Write a spec document covering these six core areas:**

*Note on the stack-shaped sections (Tech Stack, Commands, Project Structure, Testing Strategy):* in an existing codebase, these record constraints already true about the project — language, build tools, test runner, layout. In a greenfield project, they may be empty or *"TBD"* — picking the stack is a Phase 2 decision, not a spec-writing one.

1. **Objective** — What are we building and why? Who is the user? What does success look like?

2. **Commands** — Full executable commands with flags, not just tool names. Record the actual commands this project uses — they vary by stack (`npm run build`, `cargo build`, `make`, `pytest -xvs`, etc.). Cover at minimum: build, test, lint, dev/run.

3. **Project Structure** — Where source code lives, where tests go, where docs belong. Reflect the actual layout — conventions vary by language and framework (`src/` vs `lib/` vs `pkg/`, colocated tests vs separate `tests/` directory, monorepo vs single package). If the project already has a layout, document it; don't impose a new one.

4. **Code Style** — One real code snippet showing your style beats three paragraphs describing it. Include naming conventions, formatting rules, and examples of good output.

5. **Testing Strategy** — Test levels (unit / integration / E2E), where tests live, coverage expectations, which test levels are appropriate for which concerns, what counts as proof for new behavior. Framework name is a constraint recorded under Tech Stack, not a choice this section makes.

6. **Boundaries** — Three-tier system:
   - **Always do:** Run tests before commits, follow naming conventions, validate inputs
   - **Ask first:** Database schema changes, adding dependencies, changing CI config
   - **Never do:** Commit secrets, edit vendor directories, remove failing tests without approval

**Spec template:**

```markdown
# Spec: [Project/Feature Name]

## Objective
[What we're building and why. User stories or acceptance criteria.]

## Tech Stack
[Framework, language, key dependencies with versions]

## Commands
[Build, test, lint, dev — full commands]

## Project Structure
[Directory layout with descriptions]

## Code Style
[Example snippet + key conventions]

## Testing Strategy
[Framework, test locations, coverage requirements, test levels]

## Boundaries
- Always: [...]
- Ask first: [...]
- Never: [...]

## Success Criteria
[How we'll know this is done — specific, testable conditions]

## Open Questions
[Anything unresolved that needs human input]
```

**Before saving the spec, ask the user where and in what format.** Don't assume a default location. Many teams have conventions — `docs/specs/`, `design-docs/`, an RFC template, a Notion page, a GitHub issue, etc. Ask, and follow theirs rather than imposing a path. The same applies to format: PRD, RFC, lightweight one-pager, design doc — different teams use different shapes.

**Reframe instructions as success criteria.** When receiving vague requirements, translate them into concrete conditions:

```
REQUIREMENT: "Make the dashboard faster"

REFRAMED SUCCESS CRITERIA:
- Dashboard LCP < 2.5s on 4G connection
- Initial data load completes in < 500ms
- No layout shift during load (CLS < 0.1)
→ Are these the right targets?
```

This lets you loop, retry, and problem-solve toward a clear goal rather than guessing what "faster" means.

### Phase 2: Plan

With the validated spec, generate a technical implementation plan:

1. Identify the major components and their dependencies
2. Determine the implementation order (what must be built first)
3. Note risks and mitigation strategies
4. Identify what can be built in parallel vs. what must be sequential
5. Define verification checkpoints between phases

The plan should be reviewable: the human should be able to read it and say "yes, that's the right approach" or "no, change X."

### Phase 3: Tasks

Break the plan into discrete, implementable tasks:

- Each task should be completable in a single focused session
- Each task has explicit acceptance criteria
- Each task includes a verification step (test, build, manual check)
- Tasks are ordered by dependency, not by perceived importance
- No task should require changing more than ~5 files

**Task template:**
```markdown
- [ ] Task: [Description]
  - Acceptance: [What must be true when done]
  - Verify: [How to confirm — test command, build, manual check]
  - Files: [Which files will be touched]
```

### Phase 4: Implement

Execute tasks one at a time following `incremental-implementation` and `test-driven-development` skills. Use `context-engineering` to load the right spec sections and source files at each step rather than flooding the agent with the entire spec.

## Keeping the Spec Alive

The spec is a living document, not a one-time artifact:

- **Update when decisions change** — If you discover the data model needs to change, update the spec first, then implement.
- **Update when scope changes** — Features added or cut should be reflected in the spec.
- **Commit the spec** — The spec belongs in version control alongside the code.
- **Reference the spec in PRs** — Link back to the spec section that each PR implements.

## Common Rationalizations

| Rationalization | Reality |
|---|---|
| "This is simple, I don't need a spec" | Simple tasks don't need *long* specs, but they still need acceptance criteria. A two-line spec is fine. |
| "I'll write the spec after I code it" | That's documentation, not specification. The spec's value is in forcing clarity *before* code. |
| "The spec will slow us down" | A 15-minute spec prevents hours of rework. Waterfall in 15 minutes beats debugging in 15 hours. |
| "Requirements will change anyway" | That's why the spec is a living document. An outdated spec is still better than no spec. |
| "The user knows what they want" | Even clear requests have implicit assumptions. The spec surfaces those assumptions. |
| "I'll just put 'use [library/framework]' in the spec" | Implementation choices belong in Phase 2 (Plan). Phase 1 captures the problem and constraints so alternatives can be evaluated. If the choice is genuinely *given* (a hard constraint), say so explicitly — otherwise it's premature. |

## Red Flags

- Starting to write code without any written requirements
- Asking "should I just start building?" before clarifying what "done" means
- Implementing features not mentioned in any spec or task list
- Making architectural decisions without documenting them
- Skipping the spec because "it's obvious what to build"
- Spec contains implementation choices (specific libraries, frameworks, data structures, algorithms) presented as requirements
- Spec for work in an existing codebase doesn't reference existing files, patterns, or conventions

## Verification

Before proceeding to implementation, confirm:

- [ ] The spec covers all six core areas
- [ ] The human has reviewed and approved the spec
- [ ] Success criteria are specific and testable
- [ ] Boundaries (Always/Ask First/Never) are defined
- [ ] The spec is saved to a file in the repository
