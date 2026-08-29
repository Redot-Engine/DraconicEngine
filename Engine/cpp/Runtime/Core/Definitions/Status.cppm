// Status / ErrorCode - success or an error code, no payload. For operations
// that return a value-or-error, use std::expected instead.

export module core.status;

import core.stdtypes;

export namespace draco
{
    enum class ErrorCode : u32
    {
        Ok = 0,
        Unknown,
        InvalidArgument,
        OutOfRange,
        OutOfMemory,
        NotFound,
        NotSupported,
        AlreadyExists,
        Internal,
    };

    class Status
    {
    public:
        constexpr Status() = default;
        constexpr Status(ErrorCode code) : m_code(code) {}

        [[nodiscard]] constexpr ErrorCode code() const { return m_code; }
        [[nodiscard]] constexpr bool isOk() const { return m_code == ErrorCode::Ok; }
        [[nodiscard]] constexpr explicit operator bool() const { return isOk(); }

        friend constexpr bool operator==(Status a, Status b) { return a.m_code == b.m_code; }

    private:
        ErrorCode m_code = ErrorCode::Ok;
    };
}
