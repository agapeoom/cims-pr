# CIMS Project Build Guide

## Overview
This project uses CMake for build configuration. External dependencies such as FFmpeg, oneTBB, and AMR codecs are configured to be automatically downloaded and built using CMake's `ExternalProject_Add`.

## Prerequisites
Ensure the following tools are installed on your build server:
- **CMake** (version 2.8 or higher)
- **GCC/G++** (support for C++11)
- **Git** (for fetching external dependencies)
- **Make**

## Build Instructions
0. **Install Prerequisites**
   ```bash
   sudo apt-get update
   sudo apt-get install -y cmake build-essential libssl-dev
   ```

1. **Clone the repository**
   ```bash
   git clone <repository_url>
   cd cims
   ```

2. **Create a build directory**
   It is recommended to do an out-of-source build.
   ```bash
   mkdir build
   cd build
   ```

3. **Configure the project**
   Run cmake to generate the makefiles. This step configures the external projects but does not yet build them.
   ```bash
   cmake ..
   ```

4. **Build**
   Run make to start the build process. This will automatically download and compile the external dependencies (FFmpeg, etc.) before building the main project.
   ```bash
   make -j$(nproc)
   ```
   *(Note: The first build may take some time as it downloads and compiles external libraries.)*

## Deployment
After a successful build, a distribution package can be created using the `dist` target:
```bash
make dist
```
This will create a `dist/` directory in your build folder containing the binaries and configuration files ready for deployment.
