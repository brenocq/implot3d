cmake_minimum_required(VERSION 3.10)
project(ImPlot3DExample LANGUAGES CXX C)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

# Setup glad
FetchContent_Declare(
    glad
    GIT_REPOSITORY "https://github.com/Dav1dde/glad"
    GIT_TAG "v0.1.36"
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glad)

# Setup GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG "3.3.8"
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

# Setup ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY "https://github.com/ocornut/imgui"
    GIT_TAG "v1.91.9b"
    GIT_PROGRESS TRUE
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_SOURCE
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui.cpp
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_demo.cpp
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_draw.cpp
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_tables.cpp
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/imgui_widgets.cpp
	${CMAKE_BINARY_DIR}/_deps/imgui-src/backends/imgui_impl_glfw.cpp
	${CMAKE_BINARY_DIR}/_deps/imgui-src/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCE})
target_include_directories(imgui PUBLIC "${CMAKE_BINARY_DIR}/_deps/imgui-src/;${CMAKE_BINARY_DIR}/_deps/imgui-src/backends/")
target_link_libraries(imgui PUBLIC glfw glad)

# Setup ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY "https://github.com/epezent/implot"
    GIT_TAG "3da8bd34299965d3b0ab124df743fe3e076fa222"
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(implot)
set(IMPLOT_SOURCE
    ${CMAKE_BINARY_DIR}/_deps/implot-src/implot.cpp
    ${CMAKE_BINARY_DIR}/_deps/implot-src/implot_demo.cpp
    ${CMAKE_BINARY_DIR}/_deps/implot-src/implot_items.cpp
)
add_library(implot STATIC ${IMPLOT_SOURCE})
target_link_libraries(implot PUBLIC imgui)
target_include_directories(imgui PUBLIC "${CMAKE_BINARY_DIR}/_deps/implot-src/")

# Setup ImPlot3D
set(IMPLOT3D_SOURCE
    ../implot3d.cpp
    ../implot3d_demo.cpp
    ../implot3d_items.cpp
    ../implot3d_meshes.cpp
)
add_library(implot3d STATIC ${IMPLOT3D_SOURCE})
target_include_directories(implot3d PUBLIC "..")
target_link_libraries(implot3d PRIVATE imgui)

# Add the executable
set(EXAMPLE_SOURCE
    main.cpp
)
add_executable(example ${EXAMPLE_SOURCE})
target_link_libraries(example PRIVATE imgui implot implot3d glad glfw)
