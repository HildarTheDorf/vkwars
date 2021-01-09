cmake_minimum_required(VERSION 3.16)
project(vkwars)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

add_executable(vkwars main.cpp Renderer.cpp Window.cpp)
target_link_libraries(vkwars ${GLFW_LIBRARIES})