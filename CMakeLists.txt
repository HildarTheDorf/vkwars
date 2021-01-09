cmake_minimum_required(VERSION 3.16)
project(vkwars)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
pkg_search_module(VK REQUIRED vulkan)

add_executable(vkwars main.cpp Renderer.cpp RendererBase.cpp Window.cpp)
set_target_properties(vkwars PROPERTIES CXX_STANDARD 17)
target_link_libraries(vkwars ${GLFW_LIBRARIES} ${VK_LIBRARIES})