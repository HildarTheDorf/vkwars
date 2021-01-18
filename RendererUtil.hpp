#pragma once

#include <vulkan/vulkan.hpp>

inline constexpr void check_success(vk::Result result)
{
    if (vk::Result::eSuccess != result)
    {
        vk::throwResultException(result, "check_success");
    }
}

template<typename T>
constexpr T check_success(vk::ResultValue<T> result)
{
    check_success(result.result);
    return std::move(result.value);
}

inline constexpr void check_success(VkResult result)
{
    check_success(vk::Result(result));
}