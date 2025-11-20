# GitHub Configuration

This directory contains GitHub-specific configurations including CI/CD workflows.

## Continuous Integration (CI)

### Workflow: `ci.yml`

Automated testing and validation pipeline that runs on every push and pull request using Docker for consistent environments.

#### Jobs

1. **Build Docker Image** (`build-docker`)
   - Builds the unified Docker container (Ubuntu 24.04, clang-18, Python 3.12)
   - Uses GitHub Actions cache for Docker layers
   - Saves and uploads image as artifact for other jobs

2. **Format Check** (`format-check`)
   - Runs `clang-format` on all C++ source files inside Docker container
   - Fails if code is not properly formatted
   - Run locally: `docker compose run --rm dev bash -c "find src \( -name '*.cpp' -o -name '*.hpp' \) | xargs clang-format -i"`

3. **Host Build** (`build-host`)
   - Builds the project for host platform (Linux) inside Docker
   - Matrix strategy: Debug and Release configurations
   - Uploads build artifacts for use by test jobs
   - Note: `clang-tidy` static analysis runs automatically as part of the build (configured in CMakeLists.txt)

4. **Unit Tests** (`test-unit`)
   - Runs C++ unit tests using Google Test inside Docker
   - Executed via CTest
   - Tests: ZMQ transport, message encoding, dispatcher

5. **Integration Tests** (`test-integration`)
   - Runs Python integration tests using pytest inside Docker
   - Tests the full stack: C++ application + Python emulator
   - Generates code coverage reports
   - Uploads coverage to Codecov (if configured)

6. **ARM Builds** (`build-arm-cm4`, `build-stm32f3`)
   - Currently disabled (set to `if: false`)
   - Will be enabled once board implementations are complete
   - Uses same Docker container with ARM GCC toolchain

7. **CI Summary** (`summary`)
   - Aggregates results from all jobs
   - Provides single pass/fail status

#### Running CI Locally (with Docker)

To replicate CI checks locally before pushing:

```bash
cd docker

# 1. Build Docker image (one time, or when Dockerfile changes)
docker compose build

# 2. Format check
docker compose run --rm dev \
  bash -c "find src \( -name '*.cpp' -o -name '*.hpp' \) -print0 | xargs -0 clang-format --dry-run --Werror"

# 3. Build (clang-tidy runs automatically)
docker compose run --rm dev \
  bash -c "cmake --preset=host && cmake --build --preset=host --config Debug"

# 4. Unit tests
docker compose run --rm dev ctest --preset=host -C Debug --output-on-failure

# 5. Integration tests
docker compose run --rm dev \
  bash -c "cd py/host-emulator && pip install -r requirements.txt && pytest tests/ --blinky=../../build/host/bin/blinky --cov=src -v"
```

#### Configuration Notes

- **Docker Image**: Ubuntu 24.04 with clang-18, libc++, Python 3.12, ARM GCC
- **Caching**: Docker layers cached via GitHub Actions cache for fast rebuilds
- **Consistency**: CI and local development use identical Docker container
- **Environment Variables**:
  - `CC=clang-18`
  - `CXX=clang++-18`
  - `CMAKE_TOOLCHAIN_PATH=/usr`

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
