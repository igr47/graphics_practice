cmake_minimum_required(VERSION 3.15)
project(OpenGLGLadWindow)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

find_package(glfw3 REQUIRED)

add_executable(${PROJECT_NAME}
    src/test.cpp
    external/glad_generated/glad/src/glad.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src 
    external/glad_generated/glad/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL 
    glfw
)
