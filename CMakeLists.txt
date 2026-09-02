cmake_minimum_required(VERSION 3.20)

# ============================================================================
# Xbox Dashboard (build 3944) - Static Recompilation
#
# Layout follows xboxrecomp/templates/new-game: the toolkit builds libraries,
# this project builds the .exe.
# ============================================================================

project(xboxdash C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Bring-up: every call verifies the callee restored ebx/esi/edi and popped its
# return address. Must be set before add_subdirectory -- recomp_abi_violation_log
# lives in the toolkit's kernel library behind the same guard. Three compares
# per call; drop it once the D3D init chain is clean.
option(RECOMP_ABI_CHECK "Verify callee-saved registers across every call" ON)
if(RECOMP_ABI_CHECK)
    add_compile_definitions(RECOMP_ABI_CHECK)
endif()

# Bring-up: record every indirect-branch target the dashboard actually reaches,
# so the next disassembly pass can seed function detection with measurements
# instead of guesses. The dashboard's UI is vtable dispatch nearly all the way
# down, so most of its functions are reachable no other way.
option(RECOMP_ICALL_FEEDBACK "Record indirect-branch targets to icall_targets.dump" ON)
if(RECOMP_ICALL_FEEDBACK)
    add_compile_definitions(RECOMP_ICALL_FEEDBACK)
endif()

set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# CONFIGURE_DEPENDS so re-running the recompiler picks up added/removed chunks
# without a manual re-configure.
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")

add_executable(${PROJECT_NAME} WIN32
    src/main.c
    src/recomp/recomp_manual.c
    ${RECOMP_GEN_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen
    ${XBOXRECOMP_DIR}/include
    # recomp_icall_feedback.h, which recomp_types.h pulls in when the feedback
    # option above is on.
    ${XBOXRECOMP_DIR}/src/kernel
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    dxguid
    xinput
    winmm
    dbghelp
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj /w)
    target_link_options(${PROJECT_NAME} PRIVATE
        /LARGEADDRESSAWARE
        # Deep guest call chains run on the host stack.
        /STACK:16777216
        # The guest's addresses are absolute; the image must not move.
        /DYNAMICBASE:NO
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
)
