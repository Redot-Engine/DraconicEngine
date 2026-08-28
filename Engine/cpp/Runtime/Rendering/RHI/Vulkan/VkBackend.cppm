/// Vulkan implementation of Backend.
/// Creates VkInstance, enumerates physical devices, creates surfaces.

module;

#include "VkIncludes.h"
#include <vector>
#include <span>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

export module rhi.vk:backend;

import core.stdtypes;
import core.status;
import rhi;
import :adapter;
import :surface;

using namespace draco;

// Linux surface types - forward-declared to avoid header pollution. In the module
// purview (not the GMF): GCC requires the GMF to contain only #includes.
#if defined(__linux__)
extern "C" { typedef struct _XDisplay Display; }
typedef unsigned long XID;

struct VkXlibSurfaceCreateInfoKHR {
    VkStructureType sType;
    const void*     pNext;
    VkFlags         flags;
    Display*        dpy;
    XID             window;
};
#define VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR     static_cast<VkStructureType>(1000004000)
#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME                 "VK_KHR_xlib_surface"
using PFN_vkCreateXlibSurfaceKHR = VkResult(VKAPI_PTR*)(
    VkInstance, const VkXlibSurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*);

extern "C" { struct wl_display; struct wl_surface; }

struct VkWaylandSurfaceCreateInfoKHR {
    VkStructureType sType;
    const void*     pNext;
    VkFlags         flags;
    wl_display*     display;
    wl_surface*     surface;
};
#define VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR  static_cast<VkStructureType>(1000006000)
#define VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME              "VK_KHR_wayland_surface"
using PFN_vkCreateWaylandSurfaceKHR = VkResult(VKAPI_PTR*)(
    VkInstance, const VkWaylandSurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*);
#endif // __linux__

export namespace draco::rhi::vk {

/// Configuration for VK backend creation.
struct VkBackendDesc {
    bool enableValidation = false;
};

/// Vulkan implementation of Backend.
class VkBackendImpl : public Backend {
public:
    ~VkBackendImpl() override { destroyImpl(); }

    // ---- Backend interface ----

    std::span<Adapter* const> enumerateAdapters() override {
        return std::span<Adapter* const>(m_adapterPtrs.data(), m_adapterPtrs.size());
    }

    Status createSurface(void* windowHandle, void*
#if defined(__linux__)
                         displayHandle
#else
                         /*displayHandle*/
#endif
                         , Surface*& out) override {
        out = nullptr;
        if (!windowHandle) {
            logError("VkBackend: window handle is null");
            return ErrorCode::InvalidArgument;
        }

        VkSurfaceKHR vkSurface = VK_NULL_HANDLE;

#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR ci{};
        ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        ci.hinstance = ::GetModuleHandleW(nullptr);
        ci.hwnd      = reinterpret_cast<HWND>(windowHandle);

        VkResult vr = vkCreateWin32SurfaceKHR(m_instance, &ci, nullptr, &vkSurface);
        if (vr != VK_SUCCESS) {
            logErrorf("VkBackend: vkCreateWin32SurfaceKHR failed (%d)", static_cast<int>(vr));
            return ErrorCode::Unknown;
        }
#elif defined(__linux__)
        // Detect surface type from the window handles SDL3 actually provides.
        // SDL3 may choose Wayland even when DISPLAY is set (e.g. on Arch).
        // displayHandle == X11 Display* for X11, or wl_display* for Wayland.
        // We try Wayland first (if displayHandle looks like wl_display and we have the ext),
        // then fall back to X11.
        VkResult vr = VK_ERROR_INITIALIZATION_FAILED;
        bool triedWayland = false, triedX11 = false;

        // If we have Wayland ext and the display handle could be wl_display,
        // try Wayland. The SDL3 window's displayHandle returns wl_display*
        // when SDL chose Wayland, or X11 Display* when SDL chose X11.
        // We can't easily distinguish the pointer types, so we try Wayland first
        // if the extension is available and WAYLAND_DISPLAY is set, then X11.
        if (m_hasWayland && std::getenv("WAYLAND_DISPLAY")) {
            auto fn = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
                vkGetInstanceProcAddr(m_instance, "vkCreateWaylandSurfaceKHR"));
            if (fn) {
                VkWaylandSurfaceCreateInfoKHR ci{};
                ci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
                ci.display = reinterpret_cast<wl_display*>(displayHandle);
                ci.surface = reinterpret_cast<wl_surface*>(windowHandle);
                vr = fn(m_instance, &ci, nullptr, &vkSurface);
                triedWayland = true;
            }
        }

        // Fall back to X11 if Wayland failed or wasn't tried.
        if (vr != VK_SUCCESS && m_hasXlib) {
            auto fn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
                vkGetInstanceProcAddr(m_instance, "vkCreateXlibSurfaceKHR"));
            if (fn) {
                VkXlibSurfaceCreateInfoKHR ci{};
                ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
                ci.dpy    = reinterpret_cast<Display*>(displayHandle);
                ci.window = static_cast<XID>(reinterpret_cast<std::uintptr_t>(windowHandle));
                vr = fn(m_instance, &ci, nullptr, &vkSurface);
                triedX11 = true;
            }
        }

        if (vr != VK_SUCCESS) {
            logErrorf("VkBackend: surface creation failed (tried wayland=%d, x11=%d)", triedWayland, triedX11);
            return ErrorCode::Unknown;
        }
        if (vr != VK_SUCCESS) {
            logErrorf("VkBackend: surface creation failed (%d)", static_cast<int>(vr));
            return ErrorCode::Unknown;
        }
#else
        (void)displayHandle;
        logError("VkBackend: surface creation not supported on this platform");
        return ErrorCode::NotSupported;
#endif

        out = new VkSurfaceImpl(vkSurface, m_instance);
        return ErrorCode::Ok;
    }

    void destroy() override {
        destroyImpl();
        delete this;
    }

    // ---- Internal ----

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] bool validationEnabled() const { return m_validationEnabled; }

private:
    friend Status createBackend(const VkBackendDesc& desc, Backend*& out);

    Status init(bool enableValidation) {
        m_validationEnabled = enableValidation;
        // Announce validation state so a perf run can confirm the layers are OFF (they add real overhead).
        std::fprintf(stderr, "[Vulkan] validation layers: %s\n", enableValidation ? "ENABLED" : "DISABLED");

        // ---- Application info ----
        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "Draconic";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.pEngineName        = "Draconic";
        appInfo.engineVersion      = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_3;

        // ---- Extensions ----
        std::vector<const char*> extensions;
        extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#ifdef _WIN32
        extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
        {
            u32 availCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &availCount, nullptr);
            std::vector<VkExtensionProperties> avail(availCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &availCount, avail.data());
            auto hasExt = [&](const char* name) {
                for (const auto& e : avail)
                    if (std::strcmp(e.extensionName, name) == 0) return true;
                return false;
            };

            bool hasXlib    = hasExt(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
            bool hasWayland = hasExt(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);

            // Enable both surface extensions if available. The actual surface
            // type is detected at createSurface time from the window handles,
            // because SDL3 may choose Wayland even when DISPLAY is set.
            if (hasXlib) extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
            if (hasWayland) extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
            m_hasXlib = hasXlib; m_hasWayland = hasWayland;
            if (!hasXlib && !hasWayland) {
                logError("VkBackend: no surface extension available (need xlib or wayland)");
                return ErrorCode::Unknown;
            }
        }
#endif

        if (enableValidation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        // ---- Layers ----
        std::vector<const char*> layers;
        if (enableValidation) layers.push_back("VK_LAYER_KHRONOS_validation");

        // ---- Create instance ----
        VkInstanceCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo        = &appInfo;
        ci.enabledExtensionCount   = static_cast<u32>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();
        ci.enabledLayerCount       = static_cast<u32>(layers.size());
        ci.ppEnabledLayerNames     = layers.data();

        VkResult vr = vkCreateInstance(&ci, nullptr, &m_instance);
        if (vr != VK_SUCCESS) {
            logErrorf("VkBackend: vkCreateInstance failed (%d)", static_cast<int>(vr));
            return ErrorCode::Unknown;
        }

        if (enableValidation) setupDebugMessenger();

        enumeratePhysicalDevices();

        isInitialized = true;
        return ErrorCode::Ok;
    }

    void setupDebugMessenger() {
        VkDebugUtilsMessengerCreateInfoEXT ci{};
        ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = &debugCallback;

        auto pfn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (pfn) pfn(m_instance, &ci, nullptr, &m_debugMessenger);
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT /*types*/,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void* /*userData*/)
    {
        // Print straight to stderr (not only LogErrorf, whose sink may be swallowed) so validation
        // is actually visible in dev builds - the point of running with it on.
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            std::fprintf(stderr, "[Vulkan ERROR] %s\n", data->pMessage);
            logErrorf("[Vulkan ERROR] %s", data->pMessage);
        } else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            std::fprintf(stderr, "[Vulkan WARN] %s\n", data->pMessage);
            logWarningf("[Vulkan WARN] %s", data->pMessage);
        }
        return VK_FALSE;
    }

    void enumeratePhysicalDevices() {
        u32 count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0) return;

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        m_adapters.reserve(count);
        m_adapterPtrs.reserve(count);
        for (VkPhysicalDevice pd : devices) {
            auto* a = new VkAdapterImpl(pd, m_instance);
            m_adapters.push_back(a);
            m_adapterPtrs.push_back(a);
        }

        // Expose adapters best-GPU-first; callers take [0]. See Backend::enumerateAdapters.
        sortAdaptersByPreference(m_adapterPtrs);
    }

    void destroyImpl() {
        for (auto* a : m_adapters) delete a;
        m_adapters.clear();
        m_adapterPtrs.clear();

        if (m_debugMessenger != VK_NULL_HANDLE) {
            auto pfn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (pfn) pfn(m_instance, m_debugMessenger, nullptr);
            m_debugMessenger = VK_NULL_HANDLE;
        }

        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }

        isInitialized = false;
    }

    VkInstance                       m_instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT         m_debugMessenger = VK_NULL_HANDLE;
    bool                             m_validationEnabled = false;
    std::vector<VkAdapterImpl*>      m_adapters;
    std::vector<Adapter*>            m_adapterPtrs;

#if defined(__linux__)
    bool                             m_hasXlib    = false;
    bool                             m_hasWayland = false;
#endif
};

/// Creates a Vulkan backend. Caller owns the returned pointer - dispose via destroy().
[[nodiscard]] Status createBackend(const VkBackendDesc& desc, Backend*& out) {
    out = nullptr;
    auto* b = new VkBackendImpl();
    Status r = b->init(desc.enableValidation);
    if (r != ErrorCode::Ok) {
        delete b;
        return r;
    }
    out = b;
    return ErrorCode::Ok;
}

} // namespace draco::rhi::vk
