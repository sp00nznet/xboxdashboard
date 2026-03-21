# Xbox Dashboard - Recompiled Executable

# Collect generated recompiled code
file(GLOB RECOMP_GEN_SOURCES "recomp/gen/recomp_*.c")

add_executable(xboxdash WIN32
    main.c
    recomp/recomp_stubs.c
    recomp/recomp_manual.c
    ${RECOMP_GEN_SOURCES}
)

# Generated code uses register name macros
target_compile_definitions(xboxdash PRIVATE RECOMP_GENERATED_CODE)

# Suppress warnings in mechanically translated code
target_compile_options(xboxdash PRIVATE /w)

# Large stack for deep call chains in recompiled code
target_link_options(xboxdash PRIVATE
    /STACK:16777216
    /DYNAMICBASE:NO
)

target_include_directories(xboxdash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/recomp
    ${XBOXRECOMP_DIR}/src
)

# Link all xboxrecomp runtime libraries (kernel, d3d8, dsound, apu, nv2a, input)
target_link_libraries(xboxdash PRIVATE xboxrecomp)

# System libraries
target_link_libraries(xboxdash PRIVATE
    d3d11
    dxgi
    d3dcompiler
    xinput
    winmm
    bcrypt
)

# Output to bin/
set_target_properties(xboxdash PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/bin/RelWithDebInfo"
)
