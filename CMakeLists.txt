cmake_minimum_required (VERSION 3.10)
project(tweet2doom-data)

set(OpenGL_GL_PREFERENCE "GLVND")

enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS "on")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/GitVars.cmake)
include(cmake/BuildTypes.cmake)

if (EMSCRIPTEN)
    option(T2DD_WASM_SINGLE_FILE "t2dd: embed WASM inside the generated t2dd.js" ON)
else()
    if (MINGW)
        set(BUILD_SHARED_LIBS_DEFAULT OFF)
    else()
        set(BUILD_SHARED_LIBS_DEFAULT ON)
    endif()
endif()

# options

option(T2DD_ALL_WARNINGS            "T2DD: enable all compiler warnings" ON)
option(T2DD_ALL_WARNINGS_3RD_PARTY  "T2DD: enable all compiler warnings in 3rd party libs" OFF)

option(T2DD_SANITIZE_THREAD         "T2DT2DDable thread sanitizer" OFF)
option(T2DD_SANITIZE_ADDRESS        "T2DD: enable address sanitizer" OFF)
option(T2DD_SANITIZE_UNDEFINED      "T2DD: enable undefined sanitizer" OFF)

# sanitizers

if (T2DD_SANITIZE_THREAD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

if (T2DD_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

if (T2DD_SANITIZE_UNDEFINED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

# dependencies

if (EMSCRIPTEN)
    #-s USE_PTHREADS=1 \
    set(CMAKE_CXX_FLAGS " \
    --bind \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s NO_EXIT_RUNTIME=0 \
    -s USE_SDL=2 \
    -s USE_WEBGL2=1 \
    -s FULL_ES3=1 \
    -s DISABLE_EXCEPTION_CATCHING=1 \
    ")
else()
    find_package(SDL2 REQUIRED)
    string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)
endif()

add_subdirectory(third-party)

#find_package(Threads REQUIRED)

# main

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif ()

add_subdirectory(explorer)
#add_subdirectory(tests)
