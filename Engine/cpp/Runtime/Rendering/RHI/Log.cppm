// RHI logging shim. The ported backends use printf-style char* logging
// (logError / logWarning / logErrorf / logWarningf); this routes those to
// stdout/stderr. A thin layer so the large backend bodies port unchanged.

module;
#include <cstdio>
#include <cstdarg>

export module rhi:log;

export namespace draco::rhi
{
    inline void logWrite(bool error, const char* utf8)
    {
        std::FILE* out = error ? stderr : stdout;
        std::fputs(utf8, out);
        std::fputc('\n', out);
    }

    inline void logError(const char* message)   { logWrite(true,  message); }
    inline void logWarning(const char* message)  { logWrite(false, message); }
    inline void logInfo(const char* message)     { logWrite(false, message); }

    inline void logErrorf(const char* fmt, ...)
    {
        char buf[1024];
        va_list ap; va_start(ap, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        logWrite(true, buf);
    }

    inline void logWarningf(const char* fmt, ...)
    {
        char buf[1024];
        va_list ap; va_start(ap, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        logWrite(false, buf);
    }
}
