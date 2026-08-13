/// Lightweight hierarchical CPU scope profiler - the `profiler` module.
///
/// Scopes nest into a per-thread tree; at frame end every thread's samples merge into
/// the completed-frame snapshot. Timing uses a monotonic high-res counter. Instrument
/// with the DRACO_PROFILE_SCOPE macro (Profiler.h) - it compiles to nothing when
/// DRACO_PROFILING is off. Thread-safe: scopes touch only thread-local state; the
/// registry + frame swap are mutex-guarded, and the merge runs at frame end when
/// workers are idle.

module;
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

export module profiler;

import core;

using namespace draco;

namespace draco::profiler {

// NOTE: local stand-ins. Replace with core timing + text-formatting facilities once
// draco.core provides them.
namespace detail {
    inline void appendArg(std::u8string& out, std::u8string_view v) { out.append(v); }
    template <class T> requires std::is_arithmetic_v<T>
    void appendArg(std::u8string& out, T v) {
        const std::string s = std::to_string(v);
        out.append(reinterpret_cast<const char8_t*>(s.data()), s.size());
    }
}

inline void appendFormat(std::u8string& out, std::u8string_view fmt) { out.append(fmt); }

template <class A, class... Rest>
void appendFormat(std::u8string& out, std::u8string_view fmt, A&& a, Rest&&... rest) {
    const auto pos = fmt.find(u8"{}");
    if (pos == std::u8string_view::npos) { out.append(fmt); return; }
    out.append(fmt.substr(0, pos));
    detail::appendArg(out, std::forward<A>(a));
    appendFormat(out, fmt.substr(pos + 2), std::forward<Rest>(rest)...);
}

// Monotonic CPU tick counter (nanoseconds since the steady-clock epoch).
[[nodiscard]] inline u64 getTicks() noexcept {
    return static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
}
// getTicks() deltas are nanoseconds; convert to milliseconds for reports.
[[nodiscard]] inline f64 ticksToMilliseconds(u64 ticks) noexcept {
    return static_cast<f64>(ticks) / 1000000.0;
}

} // namespace draco::profiler

export namespace draco::profiler {

// One completed scope. `name` is a borrowed string literal (the macro passes literals).
struct ProfileSample {
    const char* name          = "";
    u64         startTick     = 0;   // getTicks() at scope entry
    u64         durationTicks = 0;
    u32         depth         = 0;   // nesting depth within its thread
    u32         threadIndex   = 0;
};

// A finished frame: its samples (across all threads) + the frame's wall-clock span.
struct ProfileFrame {
    u64                       frameNumber        = 0;
    u64                       frameStartTick     = 0;
    u64                       frameDurationTicks = 0;
    std::vector<ProfileSample> samples;

    [[nodiscard]] f64 frameMs() const noexcept { return ticksToMilliseconds(frameDurationTicks); }
};

// The profiler singleton. beginFrame/endFrame bracket a frame; beginScope/endScope (via the RAII
// helper) record nested scopes. completedFrame() returns the last finished frame for reporting.
class Profiler {
public:
    [[nodiscard]] static Profiler& get() noexcept { static Profiler instance; return instance; }

    void setEnabled(bool e) noexcept { m_enabled = e; }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    void beginFrame() noexcept {
        if (!m_enabled) { return; }
        m_frameStartTick = getTicks();
    }

    void endFrame() noexcept {
        if (!m_enabled) { return; }
        const u64 endTick = getTicks();
        std::scoped_lock lock(m_mutex);
        m_completed.samples.clear();
        m_completed.frameNumber        = m_frameNumber;
        m_completed.frameStartTick     = m_frameStartTick;
        m_completed.frameDurationTicks = (endTick >= m_frameStartTick) ? (endTick - m_frameStartTick) : 0;
        // Workers are idle at frame end, so draining their buffers here needs no per-thread lock.
        for (ThreadData* td : m_threads) {
            for (const ProfileSample& s : td->samples) { m_completed.samples.push_back(s); }
            td->samples.clear();
            td->stack.clear();   // defensive: any unbalanced scopes don't leak into the next frame
        }
        pushHistory(m_completed.frameMs());
        ++m_frameNumber;
    }

    void beginScope(const char* name) noexcept {
        if (!m_enabled) { return; }
        ThreadData& td = local();
        td.stack.push_back(ActiveScope{ name, getTicks(), static_cast<u32>(td.stack.size()) });
    }

    void endScope() noexcept {
        if (!m_enabled) { return; }
        ThreadData& td = local();
        if (td.stack.empty()) { return; }
        const ActiveScope a = td.stack[td.stack.size() - 1];
        td.stack.pop_back();
        const u64 now = getTicks();
        td.samples.push_back(ProfileSample{ a.name, a.startTick,
                                            (now >= a.startTick) ? (now - a.startTick) : 0, a.depth, td.index });
    }

    [[nodiscard]] const ProfileFrame& completedFrame() const noexcept { return m_completed; }

    // Rolling average of recent frame times (ms), for a stable headline number.
    [[nodiscard]] f64 averageFrameMs() const noexcept {
        if (m_historyCount == 0) { return 0.0; }
        f64 sum = 0.0;
        for (u32 i = 0; i < m_historyCount; ++i) { sum += m_history[i]; }
        return sum / static_cast<f64>(m_historyCount);
    }

    // Human-readable dump: frame headline (this frame + rolling avg) then the scope tree, indented
    // by depth and ordered by start time so parents precede children. (The P-key prints this.)
    [[nodiscard]] std::u8string buildReport() const {
        std::u8string out;
        const ProfileFrame& f = m_completed;
        appendFormat(out, u8"=== Profile: frame {}  {} ms (avg {} ms)  {} samples ===\n",
                     f.frameNumber, f.frameMs(), averageFrameMs(), f.samples.size());

        // Order by start time (so parents precede children) without mutating m_completed.
        std::vector<u32> order;
        for (u32 i = 0; i < static_cast<u32>(f.samples.size()); ++i) { order.push_back(i); }
        for (u32 i = 1; i < static_cast<u32>(order.size()); ++i) {
            const u32 v = order[i];
            u32 j = i;
            while (j > 0 && f.samples[order[j - 1]].startTick > f.samples[v].startTick) { order[j] = order[j - 1]; --j; }
            order[j] = v;
        }
        for (u32 idx : order) {
            const ProfileSample& s = f.samples[idx];
            out.append(u8"  ");
            for (u32 d = 0; d < s.depth; ++d) { out.append(u8"  "); }
            out.append(reinterpret_cast<const char8_t*>(s.name));   // scope names are ASCII (valid UTF-8)
            appendFormat(out, u8": {} ms [t{}]\n", ticksToMilliseconds(s.durationTicks), s.threadIndex);
        }
        return out;
    }

    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

private:
    Profiler() = default;
    ~Profiler() { for (ThreadData* td : m_threads) { delete td; } }

    struct ActiveScope { const char* name; u64 startTick; u32 depth; };
    struct ThreadData {
        u32                       index = 0;
        std::vector<ProfileSample> samples;   // completed scopes this frame
        std::vector<ActiveScope>   stack;     // currently-open scopes
    };

    // Per-thread state, created + registered on first use (registry guarded by the mutex).
    [[nodiscard]] ThreadData& local() {
        if (s_local == nullptr) {
            ThreadData* td = new ThreadData();
            std::scoped_lock lock(m_mutex);
            td->index = m_nextThreadIndex++;
            m_threads.push_back(td);
            s_local = td;
        }
        return *s_local;
    }

    void pushHistory(f64 ms) noexcept {
        m_history[m_historyHead] = ms;
        m_historyHead = (m_historyHead + 1) % kHistory;
        if (m_historyCount < kHistory) { ++m_historyCount; }
    }

    static constexpr u32 kHistory = 64;

    bool             m_enabled        = true;
    u64              m_frameNumber    = 0;
    u64              m_frameStartTick = 0;
    ProfileFrame     m_completed;
    std::mutex       m_mutex;                 // guards the thread registry + the frame swap
    std::vector<ThreadData*> m_threads;
    u32              m_nextThreadIndex = 0;
    f64              m_history[kHistory] = {};
    u32              m_historyHead  = 0;
    u32              m_historyCount = 0;

    inline static thread_local ThreadData* s_local = nullptr;
};

// RAII scope: brackets a profiled region. Use via DRACO_PROFILE_SCOPE (Profiler.h).
class ScopedProfile {
public:
    explicit ScopedProfile(const char* name) noexcept { Profiler::get().beginScope(name); }
    ~ScopedProfile() noexcept { Profiler::get().endScope(); }
    ScopedProfile(const ScopedProfile&) = delete;
    ScopedProfile& operator=(const ScopedProfile&) = delete;
};

} // namespace draco::profiler
