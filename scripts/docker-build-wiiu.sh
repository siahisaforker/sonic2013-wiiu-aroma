#!/bin/sh
# Build the Docker image and run it to build the Wii U binary.
set -e

docker build -t sonic-wiiu -f Dockerfile.wiiu .

# Run the container and mount the repository into /app so build artifacts land in the repo
docker run --rm -v "$(pwd)":/app sonic-wiiu
