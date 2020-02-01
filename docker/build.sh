#!/bin/sh

export DOCKER_CLI_EXPERIMENTAL=enabled

PLATFORMS="linux/386,linux/amd64,linux/arm/v6,linux/arm/v7,linux/arm64/v8,linux/ppc64le,linux/s390x"
DH_NAME="mfcoin/mfcoind"

docker run --rm --privileged docker/binfmt:66f9012c56a8316f9244ffd7622d7c21c1f6f28d

docker buildx use mfcoinbuilder || docker buildx create --use --name mfcoinbuilder

docker buildx build -f "docker/Dockerfile" -t "$DH_NAME:latest" -t "$DH_NAME:$1" "--platform=$PLATFORMS" . --push
