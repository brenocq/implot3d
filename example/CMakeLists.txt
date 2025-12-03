cmake_minimum_required(VERSION 3.10)
project(ImPlot3DExample LANGUAGES CXX C)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

# Setup OpenGL
cmake_policy(SET CMP0072 NEW) # Pefer GLVND over legacy GL libraries
find_package(OpenGL REQUIRED)

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
    GIT_TAG "v1.92.4"
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_SOURCE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCE})
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR};${imgui_SOURCE_DIR}/backends/")
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# Setup ImPlot
FetchContent_Declare(
    implot
    GIT_REPOSITORY "https://github.com/epezent/implot"
    GIT_TAG "v0.17"
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(implot)
set(IMPLOT_SOURCE
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_demo.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
add_library(implot STATIC ${IMPLOT_SOURCE})
target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)

# Setup ImPlot3D
set(IMPLOT3D_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(IMPLOT3D_SOURCE
    ${IMPLOT3D_SOURCE_DIR}/implot3d.cpp
    ${IMPLOT3D_SOURCE_DIR}/implot3d_demo.cpp
    ${IMPLOT3D_SOURCE_DIR}/implot3d_items.cpp
    ${IMPLOT3D_SOURCE_DIR}/implot3d_meshes.cpp
)
add_library(implot3d STATIC ${IMPLOT3D_SOURCE})
target_include_directories(implot3d PUBLIC ${IMPLOT3D_SOURCE_DIR})
target_link_libraries(implot3d PUBLIC imgui)

# Add the executable
set(EXAMPLE_SOURCE
    main.cpp
)
add_executable(example ${EXAMPLE_SOURCE})
target_link_libraries(example PRIVATE implot implot3d)

# Silence OpenGL deprecation warnings on macOS
if(APPLE)
    target_compile_definitions(example PRIVATE GL_SILENCE_DEPRECATION)
endif()
