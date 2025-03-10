## Overview
ANARI Project. In progress.
Serves as a playground for using the ANARI API and may evolve into something more valuable in the future.

## Requirements
- vcpkg installed, `VCPKG_ROOT` env variable should be set.
- CMake 3.19 or higher.
- C++20 supported.
- `visgl` anari device should be available (the device can be built from https://github.com/NVIDIA/VisRTX/tree/next_release).

## Build
Follow the instructions:
```bash
cmake --preset=default
cmake -B build -S ./
cmake --build build
```
