# GitHub Configuration

This directory contains GitHub-specific configurations including CI/CD workflows.

## Continuous Integration (CI)

### Workflow: `ci.yml`

Automated testing and validation pipeline that runs on every push and pull request.

#### Jobs

1. **Format Check** (`format-check`)
   - Runs `clang-format` on all C++ source files
   - Fails if code is not properly formatted
   - Run locally: `find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i`

2. **Static Analysis** (`lint`)
   - Runs `clang-tidy` static analysis
   - Enforces code quality standards defined in `.clang-tidy`
   - Configured as part of CMake build process

3. **Host Build** (`build-host`)
   - Builds the project for host platform (Linux)
   - Matrix strategy: Debug and Release configurations
   - Uploads build artifacts for use by test jobs

4. **Unit Tests** (`test-unit`)
   - Runs C++ unit tests using Google Test
   - Executed via CTest
   - Tests: ZMQ transport, message encoding, dispatcher

5. **Integration Tests** (`test-integration`)
   - Runs Python integration tests using pytest
   - Tests the full stack: C++ application + Python emulator
   - Generates code coverage reports
   - Uploads coverage to Codecov (if configured)

6. **ARM Builds** (`build-arm-cm4`, `build-stm32f3`)
   - Currently disabled (set to `if: false`)
   - Will be enabled once board implementations are complete

7. **CI Summary** (`summary`)
   - Aggregates results from all jobs
   - Provides single pass/fail status

#### Running CI Locally

To replicate CI checks locally before pushing:

```bash
# 1. Format check
find src -name '*.cpp' -o -name '*.hpp' | xargs clang-format --dry-run --Werror

# 2. Build (with clang-tidy enabled)
cmake --preset=host
cmake --build --preset=host --config Debug

# 3. Unit tests
ctest --preset=host -C Debug --output-on-failure

# 4. Integration tests
cd py/host-emulator
pytest tests/ --blinky=../../build/host/bin/blinky --cov=src -v
```

#### Configuration Notes

- **Toolchain Paths**: CI uses `/usr` as `CMAKE_TOOLCHAIN_PATH` (Linux standard location)
- **Python Version**: CI uses Python 3.11
- **Ubuntu Version**: CI runs on `ubuntu-latest`
- **Dependencies**: Installed via `apt-get` (clang, libc++, cmake, ninja, libzmq3-dev)

#### Triggers

The CI pipeline runs on:
- Push to `main`, `master`, `develop`, or any `claude/**` branch
- Pull requests targeting `main`, `master`, or `develop`
- Manual trigger via GitHub Actions UI (`workflow_dispatch`)

#### Status Badge

The CI status badge in the main README shows the build status:
```markdown
[![CI](https://github.com/nehalkpatel/embedded-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/nehalkpatel/embedded-cpp/actions/workflows/ci.yml)
```

## Future Enhancements

- [ ] Enable ARM Cortex-M4/M7 builds once board implementations are complete
- [ ] Add code coverage badges
- [ ] Add static analysis badges (clang-tidy, cppcheck)
- [ ] Add release automation
- [ ] Add Docker container builds for consistent build environments
- [ ] Add hardware-in-the-loop (HIL) testing
