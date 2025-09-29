#!/usr/bin/env bash
set -euo pipefail

: > .env

if command -v nvidia-smi >/dev/null 2>&1; then
    echo "[detect] GPU detected -> CUDA build"
    {
        echo 'BASE_IMAGE=nvidia/cuda:12.4.1-devel-ubuntu22.04'
        echo 'RUNTIME_IMAGE=nvidia/cuda:12.4.1-runtime-ubuntu22.04'
    } > .env
else
    echo "[detect] No GPU -> CPU build"
    {
        echo 'BASE_IMAGE=ubuntu:22.04'
        echo 'RUNTIME_IMAGE=ubuntu:22.04'
    } > .env
fi

echo "[ok] Wrote .env:"
cat .env

docker compose build
