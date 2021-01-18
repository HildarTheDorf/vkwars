set(AllShaderSources main.frag main.vert)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(ShaderFlags -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    set(ShaderFlags -O)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    set(ShaderFlags -Os)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    set(ShaderFlags -O -g)
else()
    set(ShaderFlags "")
endif()

set(AllShaders "")
foreach(ShaderSource ${AllShaderSources})
    set(ShaderName ${ShaderSource}.spv)
    add_custom_command(VERBATIM
        OUTPUT ${ShaderName}
        MAIN_DEPENDENCY ${ShaderSource}
        COMMAND glslc ${ShaderFlags} --target-env=vulkan1.2 ${CMAKE_CURRENT_SOURCE_DIR}/${ShaderSource} -o ${ShaderName})
    list(APPEND AllShaders ${ShaderName})
endforeach()

add_custom_target(vkwars_shaders DEPENDS ${AllShaders})
