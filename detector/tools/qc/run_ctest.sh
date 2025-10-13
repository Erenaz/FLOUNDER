#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"

cmake --build "$repo_root/detector/build" --target test_light_yield
ctest --output-on-failure -R light_yield --test-dir "$repo_root/detector/build"
