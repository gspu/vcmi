#!/usr/bin/env bash

RELEASE_TAG="1.2"
FILENAME="$1"
DOWNLOAD_URL="https://github.com/vcmi/vcmi-dependencies/releases/download/$RELEASE_TAG/$FILENAME.tgz"

mkdir ~/.conan
cd ~/.conan
curl -L "$DOWNLOAD_URL" | tar -xzf -
