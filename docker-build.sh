#!/usr/bin/env bash
set -e

# Clone Hero PSP - Docker Build Helper
IMAGE="pspdev/pspdev:latest"

echo "========================================="
echo "  Building Clone Hero PSP with Docker    "
echo "========================================="

# Ensure Docker is installed
if ! command -v docker &> /dev/null; then
    echo "Error: docker is not installed or not in PATH."
    exit 1
fi

echo "Using Docker image: $IMAGE"

# Run make inside container with host user permissions
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -v "$(pwd):/build" \
    -w /build \
    "$IMAGE" \
    make "$@"

if [ -f "EBOOT.PBP" ]; then
    echo ""
    echo "========================================="
    echo "  BUILD SUCCESSFUL!                      "
    echo "========================================="
    echo "Output: EBOOT.PBP ($(du -h EBOOT.PBP | cut -f1))"
    echo "To test on PSP/PPSSPP, copy this folder (containing EBOOT.PBP and song/) to:"
    echo "  PSP/GAME/cloneHero/"
    echo "========================================="
fi

