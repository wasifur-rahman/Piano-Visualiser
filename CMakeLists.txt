cmake_minimum_required(VERSION 3.15)
project(PianoViz CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# raylib: bypassing vcpkg here since vcpkg's raylib build has a confirmed
# rendering bug on this machine. Using the known-working build instead
# (from the raylib itch.io / w64devkit install).
set(RAYLIB_SRC "C:/raylib/raylib/src")

# RtMidi and FluidSynth: still managed by vcpkg, since these have not
# exhibited the same bug.
find_package(RtMidi CONFIG REQUIRED)
find_package(FluidSynth CONFIG REQUIRED)

add_executable(piano_viz src/main.cpp)

target_include_directories(piano_viz PRIVATE ${RAYLIB_SRC})

target_link_libraries(piano_viz PRIVATE
    RtMidi::rtmidi
    FluidSynth::libfluidsynth
    ${RAYLIB_SRC}/libraylib.a
    opengl32
    gdi32
    winmm
)
