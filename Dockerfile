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

# Set up clang alternatives to use clang-18 as default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 && \
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 && \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100

# Install uv for fast Python package management (to /usr/local/bin for all users)
RUN curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR=/usr/local/bin sh

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

