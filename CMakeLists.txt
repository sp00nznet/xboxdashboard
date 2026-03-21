cmake_minimum_required(VERSION 3.20)
project(xboxdash_recomp C)

# Xbox Dashboard - Static Recompilation
# First-ever native PC port of the original Xbox Dashboard (build 3944)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common definitions
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── xboxrecomp runtime libraries ──────────────────────────
# Point this to your xboxrecomp checkout
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp" CACHE PATH "Path to xboxrecomp toolkit")
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# ── Recompiled dashboard executable ──────────────────────
add_subdirectory(src)
