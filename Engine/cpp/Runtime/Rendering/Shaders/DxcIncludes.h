#ifndef DRACO_SHADERS_DXC_INCLUDES_H_
#define DRACO_SHADERS_DXC_INCLUDES_H_

// DXC uses __uuidof (MSVC extension) and has non-standard enum values.
// Suppress these diagnostics for all TUs that include this header.
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wlanguage-extension-token"
#  pragma clang diagnostic ignored "-Wmicrosoft-enum-value"
#endif

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <Unknwn.h>
#endif

#include <dxc/dxcapi.h>

#endif // DRACO_SHADERS_DXC_INCLUDES_H_
