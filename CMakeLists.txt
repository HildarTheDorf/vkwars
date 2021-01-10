cmake_minimum_required(VERSION 3.16)
project(vkwars)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
pkg_search_module(VK REQUIRED vulkan)

add_executable(vkwars main.cpp Renderer.cpp RendererBase.cpp Window.cpp vk_mem_alloc.cpp)
target_compile_options(vkwars PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang>: -Weverything -Wno-c++98-compat -Wno-missing-field-initializers -Wno-padded -Wno-switch-enum>
    $<$<CXX_COMPILER_ID:GNU>: -Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>: /W4>
)
target_include_directories(vkwars SYSTEM PRIVATE include)
target_link_libraries(vkwars ${GLFW_LIBRARIES} ${VK_LIBRARIES})
set_target_properties(vkwars PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
)