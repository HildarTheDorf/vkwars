cmake_minimum_required(VERSION 3.16)
project(vkwars)

include(FetchContent)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.79
)
FetchContent_MakeAvailable(imgui)

if (WIN32)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG 3.3.2
    )
    FetchContent_MakeAvailable(glfw)

    include_directories(SYSTEM $ENV{VULKAN_SDK}/Include)
    if(${CMAKE_SIZEOF_VOID_P} STREQUAL "4")
        link_directories(SYSTEM $ENV{VULKAN_SDK}/Lib32)
    else()
        link_directories(SYSTEM $ENV{VULKAN_SDK}/Lib)
    endif()
    set(PLATFORM_DEPENDENCIES vulkan-1)
else()
    find_package(PkgConfig REQUIRED)
    find_package(Threads REQUIRED)

    pkg_search_module(GLFW REQUIRED glfw3)
    pkg_search_module(VK REQUIRED vulkan)

    set(PLATFORM_DEPENDENCIES ${GLFW_LIBRARIES} ${VK_LIBRARIES} Threads::Threads)
endif()

add_library(imgui ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp ${imgui_SOURCE_DIR}/examples/imgui_impl_glfw.cpp ${imgui_SOURCE_DIR}/examples/imgui_impl_vulkan.cpp)
target_include_directories(imgui PRIVATE ${imgui_SOURCE_DIR})
target_link_libraries(imgui glfw)

add_executable(vkwars main.cpp Renderer.cpp RendererBase.cpp Window.cpp vk_mem_alloc.cpp)
target_compile_options(vkwars PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang>: -Werror -Weverything -Wno-c++98-compat -Wno-missing-field-initializers -Wno-padded -Wno-switch-enum>
    $<$<CXX_COMPILER_ID:GNU>: -Werror -Wall -Wextra -pedantic -Wno-missing-field-initializers>
    $<$<CXX_COMPILER_ID:MSVC>: /W4>
)
target_include_directories(vkwars SYSTEM PRIVATE include)
target_link_libraries(vkwars imgui ${PLATFORM_DEPENDENCIES})
set_target_properties(vkwars PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
)