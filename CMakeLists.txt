cmake_minimum_required(VERSION 3.20)
project(shader_grid_win32 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(shader_grid_win32
  src/main.c
  third_party/glad/glad.c
)
target_include_directories(shader_grid_win32 PRIVATE third_party/glad/include)
if (WIN32)
  target_link_libraries(shader_grid_win32 PRIVATE opengl32)
endif()
