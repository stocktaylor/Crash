# crash - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(crash LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(CRASH_SOURCES
    src/main.cpp
    src/crash_hooks.cpp
)

if(WIN32)
    add_executable(crash WIN32 ${CRASH_SOURCES})
else()
    add_executable(crash ${CRASH_SOURCES})
endif()

rexglue_setup_target(crash GPU_PLUGINS xenos)

# Hand-written hooks for the DLL module (DLL/CRASH.DLL.XEX): codegen calls them
# from the module's generated code, so they're built into its shared library.
# The target only exists once codegen has generated dll_targets.cmake.
if(TARGET crash_CRASH_DLL)
    target_sources(crash_CRASH_DLL PRIVATE src/crash_dll_hooks.cpp)
endif()

# In-window launch menu shared by the recomp projects (../rexlauncher).
add_subdirectory(../rexlauncher rexlauncher)
target_link_libraries(crash PRIVATE rexlauncher)
