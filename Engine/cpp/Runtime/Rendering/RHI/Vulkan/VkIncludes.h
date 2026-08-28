#ifndef DRACO_RHI_VK_INCLUDES_H_
#define DRACO_RHI_VK_INCLUDES_H_

#ifdef _WIN32
#  ifndef VK_USE_PLATFORM_WIN32_KHR
#    define VK_USE_PLATFORM_WIN32_KHR
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

#include <vulkan/vulkan.h>

#endif // DRACO_RHI_VK_INCLUDES_H_
