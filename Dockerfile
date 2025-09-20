FROM nvidia/cuda:11.8.0-devel-ubuntu22.04 as base
    RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 \
        python3-pip \
        protobuf-compiler \
        libzmq3-dev \
        ca-certificates \
        libzmq5

    WORKDIR /app

FROM base as development
    RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        git \
        gdb \
        clang \
        clang-tidy \
        cppcheck \
        python3 \
        python3-pip \
        libprotobuf-dev \
        protobuf-compiler \
        libzmq3-dev \
        ca-certificates \
        curl \
        tar \
        unzip \
        vim

    RUN python3 -m pip install --no-cache-dir \
        pytest \
        sphinx \
        breathe

FROM base AS runtime
    RUN apt-get update && apt-get install -y --no-install-recommends \
        libprotobuf23 \
        libzmq5 \
        ca-certificates \
        && rm -rf /var/lib/apt/lists/*
