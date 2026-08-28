/// Model IO - loader abstraction and registry.
/// Format-specific loaders (GLTF, FBX) register here.
/// Callers use loadModel(path, model) which selects the right loader by extension.

module;
#include <string_view>

#include <cstring>
#include <vector>

export module model:io;

import core;
import :model;

using namespace draco;

export namespace draco::model::io {

/// Abstract base for format-specific model loaders.
class ModelLoader {
public:
    virtual ~ModelLoader() = default;

    /// Check if this loader supports the given file extension (e.g. ".gltf").
    [[nodiscard]] virtual bool supportsExtension(std::u8string_view ext) const = 0;

    /// Load a model from a file path.
    virtual ModelLoadResult load(std::u8string_view path, Model& model) = 0;
};

/// Register a model loader. Does not take ownership.
void registerLoader(ModelLoader* loader);

/// Unregister a model loader.
void unregisterLoader(ModelLoader* loader);

/// Load a model from a file, selecting the appropriate loader by extension.
/// Returns UnsupportedFormat if no loader is registered for the extension.
ModelLoadResult loadModel(std::u8string_view path, Model& model);

/// Check if any loaders are registered.
[[nodiscard]] bool hasLoaders();

// ---- Implementation ----

namespace detail {
    inline std::vector<ModelLoader*>& loaders() {
        static std::vector<ModelLoader*> s;
        return s;
    }

    inline std::u8string_view getExtension(std::u8string_view path) {
        for (usize i = path.size(); i > 0; --i) {
            if (path.data()[i - 1] == '.') return std::u8string_view(path.data() + i - 1, path.size() - i + 1);
            if (path.data()[i - 1] == '/' || path.data()[i - 1] == '\\') break;
        }
        return {};
    }
}

inline void registerLoader(ModelLoader* loader) {
    if (!loader) return;
    auto& v = detail::loaders();
    for (auto* l : v) if (l == loader) return;
    v.push_back(loader);
}

inline void unregisterLoader(ModelLoader* loader) {
    auto& v = detail::loaders();
    for (usize i = 0; i < v.size(); ++i) {
        if (v[i] == loader) { v.erase(v.begin() + static_cast<std::ptrdiff_t>(i)); return; }
    }
}

inline ModelLoadResult loadModel(std::u8string_view path, Model& model) {
    std::u8string_view ext = detail::getExtension(path);
    for (auto* loader : detail::loaders()) {
        if (loader->supportsExtension(ext))
            return loader->load(path, model);
    }
    return ModelLoadResult::UnsupportedFormat;
}

inline bool hasLoaders() { return !detail::loaders().empty(); }

} // namespace draco::model::io
