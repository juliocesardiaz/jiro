# libASPL (vendored placeholder)

This directory is a placeholder for **libASPL**, the C++ library that wraps the
macOS `AudioServerPlugIn` C ABI in a modern, object-oriented API. SonicPatch's
HAL virtual audio driver (`SonicPatchDriver/`) is built on top of it.

- Upstream: https://github.com/gavv/libASPL
- License: MIT
- Notably used by **Roc Virtual Audio Device (roc-vad)** and other macOS virtual
  audio drivers, so it is a well-exercised foundation.

## How it is consumed

libASPL is **not** checked into this repo. Obtain it one of two ways:

1. **Git submodule** (recommended for reproducible builds):

   ```sh
   git submodule add https://github.com/gavv/libASPL.git Vendor/libASPL
   git submodule update --init --recursive
   ```

2. **Homebrew** (for local development on a Mac):

   ```sh
   brew install libASPL   # if/when a formula is available
   ```

The Apple-only driver target links libASPL and includes `<aspl/Driver.hpp>`,
`<aspl/Device.hpp>`, `<aspl/Stream.hpp>`, etc. On non-Apple builds the driver
sources compile only their portable portions (everything Apple-specific is
guarded by `#if defined(__APPLE__)`), so libASPL is not required for CI of the
pure-C++ engine.
