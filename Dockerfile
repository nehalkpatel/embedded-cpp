FROM mcr.microsoft.com/devcontainers/base:ubuntu24.04

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get --no-install-recommends -y full-upgrade && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    wget \
    # Clang / LLVM for host
    clang \
    clang-format \
    clang-tidy \
    lld \
    lldb \
    libc++-dev \
    libc++abi-dev \
    # Python
    python3 \
    python3-pip \
    python3-venv \
    python3-dev \
    pipx \
    # Additional tools
    libzmq3-dev \
    unzip \
    # ARM GCC toolchain
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    gdb \
    gdb-multiarch \
    neovim \
    less \
    && rm -rf /var/lib/apt/lists/*

# The LLVM major version this project builds and formats with. The other
# places that must agree read it from here in spirit: tools/format.sh defaults
# to the same value (override with CLANG_FORMAT_MAJOR), and ci.yml installs
# clang-format-<this> on the runner for the fast-fail format check.
ARG LLVM_VERSION=18

# Set up clang alternatives so the unversioned names resolve to LLVM_VERSION
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-${LLVM_VERSION} 100 && \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-${LLVM_VERSION} 100

# Install uv for fast Python package management (to /usr/local/bin for all users)
RUN curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR=/usr/local/bin sh

# Bake the project's Python into the image so builds don't download it every time.
# Ubuntu 24.04 only ships 3.12, so uv manages the interpreter instead.
ENV UV_PYTHON_INSTALL_DIR=/opt/uv-python
ENV UV_LINK_MODE=copy
RUN uv python install 3.14 && chmod -R a+rX /opt/uv-python

# ... Developer comfort tools (optional, for interactive use) ...
ARG INSTALL_DEV_TOOLS=false

RUN if [ "$INSTALL_DEV_TOOLS" = "true" ]; then \
    apt-get update && apt-get install -y \
    bat \
    fzf \
    htop \
    nano \
    ripgrep \
    tree \
    && rm -rf /var/lib/apt/lists/*; \
    fi

