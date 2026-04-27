#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
IMAGE="${KINDLE_IAGNO_DOCKER_IMAGE:-kindle-iagno-armhf-build:bullseye}"
CONTAINER="${KINDLE_IAGNO_DOCKER_CONTAINER:-kindle-iagno-armhf-builder}"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    "$ROOT/docker_build_image.sh"
fi

if docker container inspect "$CONTAINER" >/dev/null 2>&1; then
    if [ "$(docker inspect -f '{{.State.Running}}' "$CONTAINER")" != "true" ]; then
        docker start "$CONTAINER" >/dev/null
    fi
else
    docker run -d \
        --platform linux/arm/v7 \
        --name "$CONTAINER" \
        -v "$ROOT:/src/kindle-iagno" \
        -w /src/kindle-iagno \
        "$IMAGE" \
        sleep infinity >/dev/null
fi

echo "$CONTAINER"
