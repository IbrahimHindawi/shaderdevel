cmake_minimum_required(VERSION 3.20)
project(shader_grid_win32 C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(GLAD_DIR "${CMAKE_SOURCE_DIR}/third_party/glad")
add_executable(${PROJECT_NAME} WIN32
  ${CMAKE_SOURCE_DIR}/src/main.c
  ${GLAD_DIR}/src/gl.c
  ${GLAD_DIR}/src/wgl.c
)
target_include_directories(${PROJECT_NAME} PRIVATE ${GLAD_DIR}/include)
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
target_link_libraries(${PROJECT_NAME} PRIVATE opengl32 user32 gdi32)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
