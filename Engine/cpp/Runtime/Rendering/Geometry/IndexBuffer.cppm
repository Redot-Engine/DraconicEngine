/// Format-tagged 16/32-bit index buffer (`:index_buffer` partition).
///
/// A format-tagged index buffer (16- or 32-bit), stored as raw bytes so it uploads
/// straight to a GPU index buffer. An internal write cursor supports streaming
/// append (Add/AddTriangle) for the primitive generators + importers.

module;

#include <vector>
#include <cstring>

export module geometry:index_buffer;

import core;

using namespace draco;

export namespace draco::geometry {

class IndexBuffer {
public:
    enum class Format : u8 { U16, U32 };

    explicit IndexBuffer(Format format = Format::U32) : m_format(format) {}

    [[nodiscard]] Format getFormat() const noexcept { return m_format; }
    [[nodiscard]] u32 indexSize() const noexcept { return m_format == Format::U16 ? 2u : 4u; }
    [[nodiscard]] u32 count() const noexcept { return m_count; }
    [[nodiscard]] u32 dataSize() const noexcept { return m_count * indexSize(); }
    [[nodiscard]] const u8* rawData() const noexcept { return m_count > 0 ? m_data.data() : nullptr; }

    void reserve(u32 count) {
        const u32 needed = count * indexSize();
        if (m_data.size() < needed) { m_data.resize(needed); }
    }

    // Sets the logical count (growing storage) and rewinds the append cursor.
    void resize(u32 count) {
        m_count = count;
        reserve(count);
        m_writePos = 0;
    }

    void clear() { m_count = 0; m_writePos = 0; }

    void set(u32 index, u32 value) {
        if (index >= m_count) { return; }
        const u32 offset = index * indexSize();
        if (m_format == Format::U16) {
            const u16 v = static_cast<u16>(value);
            std::memcpy(m_data.data() + offset, &v, 2);
        } else {
            std::memcpy(m_data.data() + offset, &value, 4);
        }
    }

    [[nodiscard]] u32 get(u32 index) const {
        if (index >= m_count) { return 0; }
        const u32 offset = index * indexSize();
        if (m_format == Format::U16) {
            u16 v = 0; std::memcpy(&v, m_data.data() + offset, 2); return v;
        }
        u32 v = 0; std::memcpy(&v, m_data.data() + offset, 4); return v;
    }

    // Append using the internal cursor (call Resize/Reserve first).
    void add(u32 value) {
        if (m_writePos >= m_count) { return; }
        set(m_writePos, value);
        ++m_writePos;
    }
    void addTriangle(u32 a, u32 b, u32 c) { add(a); add(b); add(c); }

private:
    std::vector<u8> m_data;
    u32       m_count    = 0;
    u32       m_writePos = 0;
    Format    m_format   = Format::U32;
};

} // namespace draco::geometry
