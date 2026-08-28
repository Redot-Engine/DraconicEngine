/// Shader compilation helpers - wraps DXC compile + createShaderModule into one call.
/// Automatically selects SPIR-V (Vulkan) or DXIL (DX12) based on device type.
/// Applies Vulkan binding shifts when targeting SPIR-V.

module;

#include <span>
#include <string>
#include <string_view>

export module samples.rhi.framework:shader_helpers;

import core.stdtypes;
import core.status;
import rhi;
import shaders;

using namespace draco;

export namespace draco::samples::framework {

/// Compile HLSL source to a ShaderModule with a specific shader model.
inline Status compileToModule(shaders::Compiler* compiler, rhi::Device* device,
                              std::u8string_view hlslSource, shaders::ShaderStage stage,
                              std::u8string_view entryPoint, std::u8string_view label,
                              std::u8string_view shaderModel,
                              rhi::ShaderModule*& out) {
    out = nullptr;

    bool isDX12 = (device->type == rhi::DeviceType::DX12);
    auto target = isDX12 ? shaders::ShaderTarget::DXIL : shaders::ShaderTarget::SPIRV;

    shaders::CompileOptions opts{};
    opts.shaderModel       = shaderModel;
    opts.optimizationLevel = 3;

    // Vulkan needs binding shifts; DX12 uses register spaces natively.
    if (!isDX12) {
        opts.bindingShifts.constantBufferShift = 0;
        opts.bindingShifts.textureShift        = 1000;
        opts.bindingShifts.uavShift            = 2000;
        opts.bindingShifts.samplerShift        = 3000;
        opts.bindingShiftSets                  = 4;
    }

    shaders::CompileResult cr{};
    Status r = compiler->compile(
        reinterpret_cast<const u8*>(hlslSource.data()), hlslSource.size(),
        stage, entryPoint, target, opts, cr);

    if (r != ErrorCode::Ok) {
        if (cr.messages) {
            const std::u8string lbl = std::u8string(label);
            rhi::logErrorf("Shader compile failed (%s): %s",
                reinterpret_cast<const char*>(lbl.c_str()), cr.messages);
        }
        compiler->freeResult(cr);
        return ErrorCode::Unknown;
    }

    rhi::ShaderModuleDesc desc{};
    desc.code  = std::span<const u8>(cr.bytecode, cr.bytecodeSize);
    desc.label = label;
    r = device->createShaderModule(desc, out);

    compiler->freeResult(cr);
    return r;
}

/// Compile HLSL source to a ShaderModule. Default shader model 6.0.
inline Status compileToModule(shaders::Compiler* compiler, rhi::Device* device,
                              std::u8string_view hlslSource, shaders::ShaderStage stage,
                              std::u8string_view entryPoint, std::u8string_view label,
                              rhi::ShaderModule*& out) {
    return compileToModule(compiler, device, hlslSource, stage, entryPoint, label, u8"6_0", out);
}

} // namespace draco::samples::framework
