# Docker Development Environment

This directory contains Docker infrastructure for consistent development and CI/CD.

## Overview

The unified container includes:
- **Ubuntu 24.04 LTS** base
- **Clang 18** (host toolchain with libc++)
- **ARM GCC** (arm-none-eabi for embedded targets)
- **CMake 3.28** and Ninja
- **Python 3.12** with pip
- **ZeroMQ** libraries
- Development tools (gdb, clang-format, clang-tidy)

## Quick Start

### Build the Container

```bash
cd docker
docker compose build
```

### Interactive Development Shell

```bash
# Start an interactive development shell
docker compose run --rm dev

# Inside the container:
cmake --preset=host
cmake --build --preset=host --config Debug
ctest --preset=host -C Debug
```

### Run Emulator + Blinky

```bash
# Start both emulator and blinky application
docker compose up emulator blinky

# Stop services
docker compose down
```

## Usage Patterns

### Building for Host

```bash
docker compose run --rm dev bash -c "
  cmake --preset=host &&
  cmake --build --preset=host --config Debug
"
```

### Building for ARM (STM32F3)

```bash
docker compose run --rm dev bash -c "
  cmake --preset=stm32f3_discovery &&
  cmake --build --preset=stm32f3_discovery --config Release
"
```

### Running Tests

```bash
# C++ unit tests
docker compose run --rm dev ctest --preset=host -C Debug --output-on-failure

# Python integration tests (CMake creates venv automatically)
docker compose run --rm dev bash -c "
  cmake --preset=host &&
  cmake --build --preset=host --config Debug &&
  ctest --preset=host -C Debug -R host_emulator_test --output-on-failure
"
```

### Code Formatting

```bash
# Check formatting
docker compose run --rm dev \
  find src \( -name '*.cpp' -o -name '*.hpp' \) -print0 | \
  xargs -0 clang-format --dry-run --Werror

# Apply formatting
docker compose run --rm dev \
  find src \( -name '*.cpp' -o -name '*.hpp' \) -print0 | \
  xargs -0 clang-format -i
```

### Static Analysis

```bash
# clang-tidy runs automatically during build
docker compose run --rm dev bash -c "
  cmake --preset=host &&
  cmake --build --preset=host --config Debug
"
```

## Development Workflow

### Option 1: Interactive Shell

```bash
# Start interactive shell
docker compose run --rm dev

# Inside container:
cmake --preset=host
cmake --build --preset=host --config Debug
ctest --preset=host -C Debug
```

### Option 2: One-off Commands

```bash
# Run specific commands without entering shell
docker compose run --rm dev cmake --preset=host
docker compose run --rm dev cmake --build --preset=host --config Debug
```

### Option 3: VS Code Dev Container

Add to `.devcontainer/devcontainer.json`:
```json
{
  "name": "Embedded C++",
  "dockerComposeFile": "../docker/docker-compose.yml",
  "service": "dev",
  "workspaceFolder": "/workspace"
}
```

## Container Details

### Installed Toolchains

**Host Toolchain (Clang):**
- `clang-18` / `clang++-18`
- `libc++-18-dev` / `libc++abi-18-dev`
- `lld-18` (LLVM linker)

**Embedded Toolchain (ARM GCC):**
- `arm-none-eabi-gcc`
- `arm-none-eabi-g++`
- Supports Cortex-M4 and Cortex-M7

**Build Tools:**
- CMake 3.28
- Ninja build system

**Python:**
- Python 3.12
- pip (system-wide)

### Environment Variables

- `CC=clang-18`
- `CXX=clang++-18`
- `CMAKE_TOOLCHAIN_PATH=/usr`

### Volumes

- `..:/workspace:cached` - Source code (host mount)
- `build-cache:/workspace/build` - Build artifacts (Docker volume)

Build artifacts are stored in a Docker volume for performance. To clear:
```bash
docker compose down -v
```

## Troubleshooting

### Permission Issues

The container runs as user `developer` (UID 1000) by default. If your host UID is different:

```bash
# Rebuild with your UID
docker compose build --build-arg USER_UID=$(id -u) --build-arg USER_GID=$(id -g)
```

### Build Cache Issues

```bash
# Clear build cache
docker compose down -v

# Rebuild container from scratch
docker compose build --no-cache
```

### Network Issues (Emulator)

The emulator and blinky communicate via ZeroMQ on the `embedded-net` bridge network. If connectivity fails:

```bash
# Check network
docker network ls
docker network inspect docker_embedded-net

# Restart services
docker compose restart emulator blinky
```

## CI/CD Integration

The CI workflow uses the same Docker image:

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: ghcr.io/${{ github.repository }}/embedded-cpp:latest
```

This ensures CI and local development use identical environments.

## Python Dependency Management

**Python packages are managed via virtual environments (venv), not system-wide installation.**

### Automatic venv Creation

CMake automatically creates a Python virtual environment when building:
- Location: `build/host/host_emulator_venv/`
- Created from: `py/host-emulator/requirements.txt`
- Used by: Integration tests (via CTest)

**You don't need to manually install Python dependencies.** CMake handles this automatically:

```bash
cmake --preset=host
cmake --build --preset=host --config Debug
# Venv is created and requirements installed automatically
```

### Manual venv Usage

To use the CMake-created venv manually:

```bash
# Activate the venv
source build/host/host_emulator_venv/bin/activate

# Run emulator
cd py/host-emulator
python -m src.emulator
```

### Adding Python Dependencies

1. Update `py/host-emulator/requirements.txt`
2. Rebuild: `cmake --build --preset=host --config Debug`
3. CMake will automatically reinstall requirements

**Note:** The Dockerfile removes the `EXTERNALLY-MANAGED` marker to allow venv creation without `--break-system-packages` flag, but all Python packages should still be installed in venvs, not system-wide.

## Customization

### Adding Packages

Edit `Dockerfile` and add to the `apt-get install` command:

```dockerfile
RUN apt-get update && apt-get install -y \
    # ... existing packages
    your-new-package \
    && rm -rf /var/lib/apt/lists/*
```

### Python Dependencies

For project-wide Python dependencies, add to `py/host-emulator/requirements.txt` and rebuild.

### Different Base Image

To use a different Ubuntu version or base image, change the `FROM` line in `Dockerfile`.

## Performance Tips

1. **Use cached volumes** for build artifacts (already configured)
2. **Mount source with `:cached`** flag (already configured)
3. **Build image once**, reuse many times
4. **Layer caching**: Order Dockerfile from least to most frequently changed

## Security Notes

- Container runs as non-root user `developer`
- No privileged mode required
- Network isolation via Docker networks
- Build artifacts isolated in Docker volumes

## Further Reading

- [Docker Compose Documentation](https://docs.docker.com/compose/)
- [CMake Presets Documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [Project CLAUDE.md](../CLAUDE.md) - Comprehensive project documentation
