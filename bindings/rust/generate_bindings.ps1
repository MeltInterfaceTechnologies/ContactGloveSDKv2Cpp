param(
    [string]$Bindgen = "bindgen",
    [string]$LibClangPath = $env:LIBCLANG_PATH
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$header = Join-Path $root "include\contactglove\ContactGloveSDKv2C.h"
$output = Join-Path $PSScriptRoot "contactglove-sdk-v2-sys\src\bindings.rs"

if ($LibClangPath) {
    $libclang = Join-Path $LibClangPath "libclang.dll"
    if (-not (Test-Path $libclang)) {
        throw "libclang.dll was not found in: $LibClangPath"
    }
    $env:LIBCLANG_PATH = $LibClangPath
}

& $Bindgen $header `
    --allowlist-function "cg_.*" `
    --allowlist-type "cg_.*" `
    --allowlist-var "(CG_|CONTACTGLOVE_).*" `
    --no-layout-tests `
    --use-core `
    -- -x c |
    Set-Content -Encoding utf8 $output
