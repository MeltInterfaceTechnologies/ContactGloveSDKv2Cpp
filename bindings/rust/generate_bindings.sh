#!/usr/bin/env sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
bindgen "${root}/include/contactglove/ContactGloveSDKv2C.h" \
  --allowlist-function 'cg_.*' \
  --allowlist-type 'cg_.*' \
  --allowlist-var '(CG_|CONTACTGLOVE_).*' \
  --no-layout-tests \
  --use-core \
  -- -x c > "$(dirname -- "$0")/contactglove-sdk-v2-sys/src/bindings.rs"
