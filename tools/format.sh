#!/usr/bin/env bash
#
# Single source of truth for formatting. CI, the CMake `format`/`format-check`
# targets, and the pre-commit hook all call this script, so the three cannot
# drift apart -- which is the whole point. A local "it's formatted" that CI
# disagrees with is worse than no check at all.
#
# Usage:
#   tools/format.sh --check     Verify the whole tree. What CI runs.
#   tools/format.sh --fix       Reformat in place, and apply safe lint fixes.
#   tools/format.sh --staged    Verify only staged content. What the hook runs.
#
# Covers what can be fixed automatically: clang-format for C++, ruff format and
# ruff's fixable lint rules for Python. Type checking (mypy) is deliberately not
# here -- it is not a formatting concern and is too slow for a commit hook.

set -uo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly PY_DIR="${REPO_ROOT}/py/host-emulator"

# Pinned to match .github/workflows/ci.yml and the dev container, which aliases
# clang-format -> clang-format-18. Versions disagree about formatting, so an
# unpinned binary produces exactly the local/CI split this script exists to
# prevent.
readonly REQUIRED_CLANG_FORMAT_MAJOR=18

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
bold()  { printf '\033[1m%s\033[0m\n' "$*"; }

find_clang_format() {
  local candidate
  for candidate in "clang-format-${REQUIRED_CLANG_FORMAT_MAJOR}" clang-format; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      local version
      version="$("${candidate}" --version | grep -oE '[0-9]+' | head -1)"
      if [[ "${version}" != "${REQUIRED_CLANG_FORMAT_MAJOR}" ]]; then
        red "warning: ${candidate} is version ${version}, expected ${REQUIRED_CLANG_FORMAT_MAJOR}." >&2
        red "         Formatting may disagree with CI." >&2
      fi
      echo "${candidate}"
      return 0
    fi
  done
  red "error: clang-format not found (want clang-format-${REQUIRED_CLANG_FORMAT_MAJOR})" >&2
  return 1
}

cpp_sources() {
  find "${REPO_ROOT}/src" \( -name '*.cpp' -o -name '*.hpp' \) -print0
}

check_all() {
  local clang_format status=0
  clang_format="$(find_clang_format)" || return 1

  bold "Checking C++ formatting (${clang_format})"
  if ! cpp_sources | xargs -r0 "${clang_format}" --dry-run --Werror; then
    red "C++ formatting errors. Fix with: tools/format.sh --fix"
    status=1
  fi

  bold "Checking Python formatting and lint (ruff)"
  if ! (cd "${PY_DIR}" && uv run --frozen ruff format --check .); then
    red "Python formatting errors. Fix with: tools/format.sh --fix"
    status=1
  fi
  if ! (cd "${PY_DIR}" && uv run --frozen ruff check .); then
    red "Python lint errors. Fix with: tools/format.sh --fix"
    status=1
  fi

  [[ ${status} -eq 0 ]] && green "All formatting checks passed."
  return ${status}
}

fix_all() {
  local clang_format
  clang_format="$(find_clang_format)" || return 1

  bold "Formatting C++ (${clang_format})"
  cpp_sources | xargs -r0 "${clang_format}" -i

  bold "Formatting Python and applying safe lint fixes (ruff)"
  (cd "${PY_DIR}" && uv run --frozen ruff format .)
  # --fix applies only rules ruff considers safe; anything else still needs a
  # human, and --check will still fail on it.
  (cd "${PY_DIR}" && uv run --frozen ruff check --fix .)

  green "Done. Review the changes before committing."
}

# Checks the *staged* content, not the working tree. A file can be staged in one
# state and edited further on disk; committing the staged version while checking
# the file on disk would pass a commit that CI then rejects.
check_staged() {
  local clang_format status=0 file staged_files
  staged_files="$(git diff --cached --name-only --diff-filter=ACMR)"
  [[ -z "${staged_files}" ]] && return 0

  local cpp_failed=() py_failed=()

  while IFS= read -r file; do
    [[ -z "${file}" ]] && continue
    case "${file}" in
      src/*.cpp|src/*.hpp|src/**/*.cpp|src/**/*.hpp)
        if [[ -z "${clang_format:-}" ]]; then
          clang_format="$(find_clang_format)" || return 1
        fi
        if ! git show ":${file}" \
             | "${clang_format}" --assume-filename="${file}" --dry-run --Werror \
               >/dev/null 2>&1; then
          cpp_failed+=("${file}")
          status=1
        fi
        ;;
      *.py)
        if ! git show ":${file}" \
             | (cd "${PY_DIR}" && uv run --frozen ruff format --check \
                  --stdin-filename "${file}" -) >/dev/null 2>&1; then
          py_failed+=("${file}")
          status=1
        elif ! git show ":${file}" \
               | (cd "${PY_DIR}" && uv run --frozen ruff check \
                    --stdin-filename "${file}" -) >/dev/null 2>&1; then
          py_failed+=("${file}")
          status=1
        fi
        ;;
    esac
  done <<< "${staged_files}"

  if [[ ${status} -ne 0 ]]; then
    red "Formatting problems in staged files:"
    for file in "${cpp_failed[@]:-}" "${py_failed[@]:-}"; do
      [[ -n "${file}" ]] && printf '    %s\n' "${file}"
    done
    printf '\n'
    printf '  Fix:    tools/format.sh --fix && git add -u\n'
    printf '  Bypass: git commit --no-verify\n'
  fi
  return ${status}
}

case "${1:---check}" in
  --check)  check_all ;;
  --fix)    fix_all ;;
  --staged) check_staged ;;
  *)
    red "usage: $0 [--check|--fix|--staged]" >&2
    exit 2
    ;;
esac
