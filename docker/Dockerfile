# Unified build container for embedded-cpp BSP
# Supports both host development (Clang) and ARM embedded builds (GCC ARM)
FROM ubuntu:24.04

LABEL maintainer="embedded-cpp"
LABEL description="Unified development environment for embedded C++ with host and ARM toolchains"

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Set working directory
WORKDIR /workspace

# Install base dependencies in a single layer
RUN apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    ninja-build \
    git \
    curl \
    wget \
    # Clang/LLVM toolchain (host builds)
    clang-18 \
    clang-format-18 \
    clang-tidy-18 \
    libc++-18-dev \
    libc++abi-18-dev \
    lld-18 \
    # Python
    python3.12 \
    python3.12-dev \
    python3.12-venv \
    python3-pip \
    # ZeroMQ and dependencies
    libzmq3-dev \
    # ARM GCC toolchain (embedded builds)
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    # Useful tools
    gdb \
    gdb-multiarch \
    vim \
    less \
    && rm -rf /var/lib/apt/lists/*

# Set up clang alternatives to use clang-18 as default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 && \
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-18 100 && \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-18 100

# Set up python3 to point to python3.12
RUN update-alternatives --install /usr/bin/python3 python3 /usr/bin/python3.12 100 && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3.12 100

# Upgrade pip and install Python dependencies
RUN python3 -m pip install --upgrade pip setuptools wheel --break-system-packages

# Create a non-root user for development
ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME && \
    useradd --uid $USER_UID --gid $USER_GID -m $USERNAME && \
    apt-get update && \
    apt-get install -y sudo && \
    echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME && \
    chmod 0440 /etc/sudoers.d/$USERNAME && \
    rm -rf /var/lib/apt/lists/*

# Set environment variables
ENV CC=clang-18
ENV CXX=clang++-18
ENV CMAKE_TOOLCHAIN_PATH=/usr
ENV PATH="/home/${USERNAME}/.local/bin:${PATH}"

# Switch to non-root user
USER $USERNAME

# Set working directory for user
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
