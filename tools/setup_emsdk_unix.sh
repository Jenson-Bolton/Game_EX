#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-latest}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_DIR="$ROOT/third_party/emsdk"

pushd "$EMSDK_DIR" >/dev/null
./emsdk install "$VERSION"
./emsdk activate "$VERSION"
source ./emsdk_env.sh
echo "EMSDK ready at $EMSDK_DIR"
popd >/dev/null
