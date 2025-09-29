ARG BASE_IMAGE=ubuntu:22.04
ARG RUNTIME_IMAGE=ubuntu:22.04

FROM ${BASE_IMAGE} AS base
    WORKDIR /app

FROM base AS development
    RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        python3 \
        python3-pip \
        protobuf-compiler \
        libzmq3-dev \
        cmake \
        pkg-config \
        git \
        gdb \
        clang \
        clang-tidy \
        cppcheck \
        doxygen \
        libprotobuf-dev \
        ca-certificates \
        curl \
        tar \
        unzip \
        vim \
    && rm -rf /var/lib/apt/lists/*

    RUN python3 -m pip install --no-cache-dir \
        pytest \
        sphinx \
        breathe

FROM development AS build
    COPY . /app

    RUN echo "Cleaning old build directory..." \
        && if [ -d "/app/build" ]; then rm -rf /app/build; fi \
        && echo "Creating new build directory..." \
        && mkdir -p /app/build \
        && echo "Running CMake configuration..." \
        && cd /app/build \
        && cmake -DCMAKE_BUILD_TYPE=Release .. \
        && echo "Building the project..." \
        && cmake --build . --parallel 8 \
        && echo "Running tests..." \
        && ctest --output-on-failure \
        && echo "Installing the build..." \
        && cmake --install . \
        && echo "Packaging the release..." \
        && cd "$(ls -dt /app/release/*/ | head -1)" \
        && tar -czvf /app/latest_release.tar.gz .

FROM ${RUNTIME_IMAGE} AS release
    WORKDIR /app
    COPY --from=build /app/latest_release.tar.gz /app/
    RUN tar -xzvf /app/latest_release.tar.gz -C /app && rm /app/latest_release.tar.gz

    RUN apt-get update && apt-get install -y --no-install-recommends \
        libprotobuf23 \
        libzmq5 \
        ca-certificates \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

    ENV LD_LIBRARY_PATH=/app:$LD_LIBRARY_PATH
