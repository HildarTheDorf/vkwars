#pragma once

#include <vulkan/vulkan.h>

#include <exception>

class BadVkResult final : public std::exception
{
public:
    BadVkResult(VkResult vkResult);

    virtual const char *what() const noexcept override;
};

constexpr void check_success(VkResult vkResult)
{
    if (vkResult)
    {
        throw BadVkResult(vkResult);
    }
}
