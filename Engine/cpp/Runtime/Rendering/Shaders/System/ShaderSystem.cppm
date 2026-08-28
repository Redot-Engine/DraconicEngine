/// The `shaders.system` module.
///
/// Compile-on-demand + cache for shader VARIANTS. A shader is registered by name
/// per stage (its HLSL source); getVariant(name, stage, flags) compiles the
/// permutation (flags -> #defines) via DXC, creates the GPU ShaderModule, and
/// caches it by (nameHash, stage, flags). This is the layer above the stateless
/// shaders Compiler that the material/PSO layers build on. Needs the RHI to
/// create modules, so it is separate from the RHI-free shaders module.

module;

#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

export module shaders.system;

import core.stdtypes;
import core.status;
import rhi;
import shaders;

using namespace draco;
namespace rhi = draco::rhi;

export namespace draco::shaders {

// Hash for the variant key so it can key an unordered_map (ShaderVariantKey is a
// trivially-copyable 16-byte struct with operator==).
struct ShaderVariantKeyHash {
    usize operator()(const ShaderVariantKey& k) const noexcept {
        u64 h = k.nameHash;
        h = (h * 1099511628211ull) ^ static_cast<u64>(k.stage);
        h = (h * 1099511628211ull) ^ static_cast<u64>(k.flags);
        return static_cast<usize>(h);
    }
};

// Compile-on-demand variant cache. The Compiler and Device are borrowed (owned by
// the caller). Sources are registered per (name, stage) - vertex and fragment are
// separate HLSL with `main` entry points.
class ShaderSystem {
public:
    ShaderSystem(Compiler& compiler, rhi::Device& device) noexcept
        : m_compiler(&compiler), m_device(&device) {}

    ~ShaderSystem() { destroyAll(); }

    ShaderSystem(const ShaderSystem&) = delete;
    ShaderSystem& operator=(const ShaderSystem&) = delete;

    // Register a shader's HLSL source for a stage (owned copy).
    void registerSource(std::u8string_view name, ShaderStage stage, std::u8string_view hlsl)
    {
        m_sources.insert_or_assign(sourceKey(name, stage), std::u8string(hlsl));
    }

    // Include search paths for DXC #include resolution of shared .hlsli (owned).
    void setIncludePaths(std::span<const std::u8string_view> paths)
    {
        m_includePaths.clear();
        for (usize i = 0; i < paths.size(); ++i) { m_includePaths.push_back(std::u8string(paths[i])); }
    }

    // Get (compile-on-demand + cache) the GPU module for a variant. Returns null
    // if the source is unknown or compilation fails (failures are NOT cached, so a
    // later request retries - e.g. after a fix).
    [[nodiscard]] rhi::ShaderModule* getVariant(std::u8string_view name, ShaderStage stage, ShaderFlags flags)
    {
        const ShaderVariantKey key{ shaderNameHash(name), stage, flags };
        if (auto it = m_cache.find(key); it != m_cache.end()) { return it->second; }

        auto sit = m_sources.find(sourceKey(name, stage));
        if (sit == m_sources.end()) { return nullptr; }

        rhi::ShaderModule* module = compile(sit->second, stage, flags);
        if (module == nullptr) { return nullptr; }

        m_cache.insert_or_assign(key, module);
        return module;
    }

    // Drop + destroy every cached variant of a shader and BUMP its version (the
    // reload signal consumers poll). Call on a shader reload. The next getVariant
    // recompiles. Returns how many variants were invalidated.
    usize invalidateShader(std::u8string_view name)
    {
        const u64 nameHash = shaderNameHash(name);
        std::vector<ShaderVariantKey> toRemove;
        for (auto& [k, v] : m_cache)
        {
            if (k.nameHash == nameHash)
            {
                if (v != nullptr) { m_device->destroyShaderModule(v); }
                toRemove.push_back(k);
            }
        }
        for (const ShaderVariantKey& k : toRemove) { m_cache.erase(k); }
        bumpVersion(nameHash);
        return toRemove.size();
    }

    // Monotonic version of a shader: bumped each invalidateShader (i.e. each
    // reload). The PSO cache stamps pipelines with this and rebuilds when it
    // changes. 0 if the shader was never registered/invalidated.
    [[nodiscard]] u64 version(std::u8string_view name) noexcept
    {
        auto it = m_versions.find(shaderNameHash(name));
        return (it != m_versions.end()) ? it->second : 0ull;
    }

private:
    [[nodiscard]] rhi::ShaderModule* compile(std::u8string_view source, ShaderStage stage, ShaderFlags flags)
    {
        const bool isDX12 = (m_device->type == rhi::DeviceType::DX12);
        const ShaderTarget target = isDX12 ? ShaderTarget::DXIL : ShaderTarget::SPIRV;

        std::vector<ShaderDefine> defines;
        appendDefines(flags, defines);

        std::vector<std::u8string_view> includeViews;
        for (const std::u8string& p : m_includePaths) { includeViews.push_back(p); }

        CompileOptions opts{};
        opts.shaderModel       = u8"6_0";
        opts.optimizationLevel = 3;
        opts.defines           = std::span<const ShaderDefine>(defines.data(), defines.size());
        opts.includePaths      = std::span<const std::u8string_view>(includeViews.data(), includeViews.size());
        if (!isDX12)
        {
            // Vulkan: shift register spaces so HLSL b/t/u/s registers don't collide
            // in SPIR-V (matches the sample framework's compileToModule).
            opts.bindingShifts.constantBufferShift = 0;
            opts.bindingShifts.textureShift        = 1000;
            opts.bindingShifts.uavShift            = 2000;
            opts.bindingShifts.samplerShift        = 3000;
            opts.bindingShiftSets                  = 4;
        }

        CompileResult cr{};
        const Status r = m_compiler->compile(
            reinterpret_cast<const u8*>(source.data()), source.size(),
            stage, u8"main", target, opts, cr);

        if (r != ErrorCode::Ok || !cr.success)
        {
            if (cr.messages != nullptr) { rhi::logErrorf("Shader variant compile failed: %s", cr.messages); }
            m_compiler->freeResult(cr);
            return nullptr;
        }

        rhi::ShaderModuleDesc desc{};
        desc.code = std::span<const u8>(cr.bytecode, cr.bytecodeSize);
        rhi::ShaderModule* module = nullptr;
        const Status mr = m_device->createShaderModule(desc, module);
        m_compiler->freeResult(cr);
        return (mr == ErrorCode::Ok) ? module : nullptr;
    }

    void destroyAll()
    {
        for (auto& kv : m_cache) { if (kv.second != nullptr) { m_device->destroyShaderModule(kv.second); } }
        m_cache.clear();
    }

    [[nodiscard]] static u64 sourceKey(std::u8string_view name, ShaderStage stage) noexcept
    {
        return (shaderNameHash(name) * 1099511628211ull) ^ static_cast<u64>(stage);
    }

    void bumpVersion(u64 nameHash)
    {
        ++m_versions[nameHash];   // default-constructs to 0, then increments
    }

    Compiler*    m_compiler;   // borrowed
    rhi::Device* m_device;     // borrowed
    std::unordered_map<u64, std::u8string> m_sources;                                        // (name,stage) -> HLSL
    std::unordered_map<ShaderVariantKey, rhi::ShaderModule*, ShaderVariantKeyHash> m_cache;  // variant -> GPU module (owned)
    std::unordered_map<u64, u64> m_versions;                                                 // nameHash -> version
    std::vector<std::u8string> m_includePaths;
};

} // namespace draco::shaders
