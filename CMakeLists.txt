cmake_minimum_required(VERSION 3.20)
project(rearview_camera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio imgcodecs)

add_executable(rearcam
  src/main.cpp
  src/app.cpp
  src/camera.cpp
  src/gl_utils.cpp
  src/guidelines.cpp
)

target_include_directories(rearcam PRIVATE src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(rearcam PRIVATE OpenGL::GL glfw glm::glm ${OpenCV_LIBS})

# Shaders are loaded from disk at runtime so they can be edited without a rebuild.
target_compile_definitions(rearcam PRIVATE SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")

if(APPLE)
  # Apple deprecated OpenGL in macOS 10.14; it still works (4.1 core, on top of Metal).
  target_compile_definitions(rearcam PRIVATE GL_SILENCE_DEPRECATION)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(rearcam PRIVATE -Wall -Wextra -Wpedantic)
endif()
