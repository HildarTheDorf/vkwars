cmake_minimum_required(VERSION 3.16)
project(vkwars)

include(FetchContent)

FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui GIT_TAG v1.79)
FetchContent_MakeAvailable(imgui)

add_subdirectory(shaders)

add_library(imgui ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_demo.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/examples/imgui_impl_glfw.cpp)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})

add_executable(vkwars main.cpp Renderer.cpp UIRenderer.cpp Uploader.cpp Window.cpp vma/Allocation.cpp vma/Allocator.cpp vma/vk_mem_alloc.cpp)
add_dependencies(vkwars vkwars_shaders)
set_target_properties(vkwars PROPERTIES CXX_STANDARD 17)
target_include_directories(vkwars PRIVATE ${imgui_SOURCE_DIR}/examples)
target_link_libraries(vkwars imgui glfw vulkan)
