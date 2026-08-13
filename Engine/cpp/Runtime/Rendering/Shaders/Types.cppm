/// Shader compilation types.

module;

#include <span>
#include <string_view>

export module shaders:types;

import core.stdtypes;

using namespace draco;

export namespace draco::shaders {

enum class ShaderStage : u32 {
    Vertex, Fragment, Compute, Mesh, Task,
    RayGen, ClosestHit, AnyHit, Miss, Intersection, Callable,
};

enum class ShaderTarget : u32 {
    SPIRV,
    DXIL,
};

struct ShaderDefine {
    std::u8string_view name;
    std::u8string_view value;
};

struct BindingShifts {
    u32 constantBufferShift = 0;
    u32 textureShift        = 0;
    u32 samplerShift        = 0;
    u32 uavShift            = 0;
};

struct CompileOptions {
    std::u8string_view shaderModel      = u8"6_0";
    i32        optimizationLevel = 3;
    bool       enableDebugInfo   = false;
    bool       rowMajorMatrices  = false;
    std::span<const ShaderDefine>  defines;
    std::span<const std::u8string_view>    includePaths;
    BindingShifts bindingShifts;
    u32           bindingShiftSets = 1;
};

struct CompileResult {
    u8*   bytecode      = nullptr;
    usize bytecodeSize  = 0;
    char* messages      = nullptr;
    usize messagesSize  = 0;
    bool  success       = false;
};

} // namespace draco::shaders
