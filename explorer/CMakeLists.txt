if (T2DD_ALL_WARNINGS)
    if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    else()
        # todo : windows
    endif()
endif()

#
## Main

set(TARGET t2d-explorer)

add_executable(${TARGET}
    core/assets.cpp
    core/frame-buffer.cpp
    core/image.cpp
    core/shader-program.cpp
    core/shader.cpp
    core/uniform-buffer.cpp
    core/utils.cpp
    common.cpp
    main.cpp
    )

target_include_directories(${TARGET} PUBLIC
    .
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}-extra/
    )

target_link_libraries(${TARGET} PRIVATE
    imgui-sdl2
    ${CMAKE_DL_LIBS}
    #${CMAKE_THREAD_LIBS_INIT}
    )

make_directory(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}-extra/)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_timestamp-tmpl.h   ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}-extra/build_timestamp.h @ONLY)

if (EMSCRIPTEN)
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS " \
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../fonts@/ \
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../images@/ \
        ")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}-extra/index.html @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/style.css                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}-extra/style.css COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/background-0.png         ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}-extra/background-0.png COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gghelpers.js             ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}-extra/gghelpers.js COPYONLY)
else()
endif()
