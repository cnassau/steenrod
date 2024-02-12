#!/bin/bash

DIR=$(realpath $(dirname "$0"))

podman build -t steenrod-build-alpine:1  -f Dockerfile.alpine

podman run --rm -it --mount type=bind,source=${DIR}/..,target=/src --mount type=bind,source=${DIR},target=/out steenrod-build-alpine


