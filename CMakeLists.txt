cmake_minimum_required(VERSION 3.15)
project(PianoViz CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
# On Windows, easiest path is vcpkg:
#   vcpkg install rtmidi raylib fluidsynth
# Then configure with:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]/scripts/buildsystems/vcpkg.cmake

find_package(RtMidi CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)
find_package(FluidSynth CONFIG REQUIRED)

add_executable(piano_viz src/main.cpp)

target_link_libraries(piano_viz PRIVATE RtMidi::rtmidi raylib FluidSynth::libfluidsynth)
