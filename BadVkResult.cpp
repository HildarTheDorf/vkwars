#include "BadVkResult.hpp"

BadVkResult::BadVkResult(VkResult)
{

}

const char *BadVkResult::what() const noexcept
{
    return "Bad VkResult";
}
