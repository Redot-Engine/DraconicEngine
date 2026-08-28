#ifndef DRACO_RHI_DX12_INCLUDES_H_
#define DRACO_RHI_DX12_INCLUDES_H_

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif

// DX12 headers use __uuidof (MSVC extension) via IID_PPV_ARGS. Clang supports
// it but warns under -Wlanguage-extension-token; suppress for all DX12 code.
// NOTE: We push but do NOT pop here - the suppression must remain active for
// IID_PPV_ARGS expansions in our .cppm files. The diagnostic state is scoped
// to each translation unit, so it won't leak.
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif

#include <windows.h>
#include <wrl/client.h>     // ComPtr
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

// Helper alias.
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#endif // DRACO_RHI_DX12_INCLUDES_H_
