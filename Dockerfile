FROM ubuntu:22.04 as base
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

FROM base AS release
    COPY --from=build /app/latest_release.tar.gz /app/
    RUN tar -xzvf /app/latest_release.tar.gz -C /app
    RUN rm latest_release.tar.gz

    RUN apt-get update && apt-get install -y --no-install-recommends \
        libprotobuf23 \
        libzmq5 \
        ca-certificates

    RUN export LD_LIBRARY_PATH=/app:$LD_LIBRARY_PATH
    RUN apt-get clean && rm -rf /var/lib/apt/lists/*

