# SonicPatchTests/CMakeLists.txt
#
# Host-side unit tests for the *portable* (no Core Audio) parts of the engine:
# the DSP, the buffer pool, the topological sort, the lock-free structures, and
# the graph publish/swap. Apple-only translation units (anything that touches
# Core Audio / AudioToolbox / libASPL, or the .mm files) are deliberately
# EXCLUDED so this builds and runs on Linux/macOS CI without the Apple SDKs.
#
# Usage:
#   cmake -S SonicPatchTests -B build-tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

cmake_minimum_required(VERSION 3.16)
project(SonicPatchTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- GoogleTest via FetchContent -------------------------------------------
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# For Windows: prevent overriding the parent project's compiler/linker settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# --- Engine sources under test (portable subset only) ----------------------
set(ENGINE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../SonicPatchEngine)

set(ENGINE_PORTABLE_SOURCES
    ${ENGINE_ROOT}/Core/BufferPool.cpp
    ${ENGINE_ROOT}/Core/TopologicalSort.cpp
    ${ENGINE_ROOT}/Core/AudioGraph.cpp
    ${ENGINE_ROOT}/DSP/Biquad.cpp
    ${ENGINE_ROOT}/DSP/ParametricEQ.cpp
    ${ENGINE_ROOT}/DSP/Compressor.cpp
    ${ENGINE_ROOT}/DSP/Limiter.cpp
    ${ENGINE_ROOT}/DSP/NoiseGate.cpp
    ${ENGINE_ROOT}/DSP/Filters.cpp
    ${ENGINE_ROOT}/DSP/GainUtility.cpp
    ${ENGINE_ROOT}/DSP/EffectFactory.cpp
    ${ENGINE_ROOT}/Nodes/MixerNode.cpp
    ${ENGINE_ROOT}/Nodes/SplitterNode.cpp
    ${ENGINE_ROOT}/Nodes/VolumeNode.cpp
    ${ENGINE_ROOT}/Nodes/MeterNode.cpp
    ${ENGINE_ROOT}/Nodes/EffectNode.cpp
    ${ENGINE_ROOT}/Nodes/TapSourceNode.cpp
    ${ENGINE_ROOT}/Nodes/DeviceSinkNode.cpp
)
# Excluded: AU/AudioUnitHost.mm (requires Apple frameworks).
# Added conditionally below: Transport/SharedMemory.cpp (POSIX-only) and
# Public/AudioEngine.cpp (portable C++, safe everywhere).
if(UNIX)
    list(APPEND ENGINE_PORTABLE_SOURCES ${ENGINE_ROOT}/Transport/SharedMemory.cpp)
endif()
list(APPEND ENGINE_PORTABLE_SOURCES ${ENGINE_ROOT}/Public/AudioEngine.cpp)

add_library(sonicpatch_engine_portable STATIC ${ENGINE_PORTABLE_SOURCES})
target_include_directories(sonicpatch_engine_portable PUBLIC ${ENGINE_ROOT})
if(UNIX AND NOT APPLE)
    # shm_open/mmap may live in librt on some Linux distributions.
    target_link_libraries(sonicpatch_engine_portable PUBLIC rt)
endif()

# --- Test executable --------------------------------------------------------
add_executable(sonicpatch_tests
    EngineTests/BufferPoolTests.cpp
    EngineTests/TopologicalSortTests.cpp
    EngineTests/AudioGraphTests.cpp
    EngineTests/AudioEngineTests.cpp
    EngineTests/MeterNodeTests.cpp
    EngineTests/MixerNodeTests.cpp
    DSPTests/BiquadTests.cpp
)
target_link_libraries(sonicpatch_tests
    PRIVATE sonicpatch_engine_portable GTest::gtest_main
)

gtest_discover_tests(sonicpatch_tests)
