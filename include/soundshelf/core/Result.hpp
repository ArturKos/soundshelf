#pragma once

#include <QString>
#include <optional>
#include <variant>
#include <utility>

namespace soundshelf {

/// Error type with code and human-readable message.
struct Error {
    enum Code {
        Unknown,
        FileNotFound,
        FileAccessDenied,
        InvalidFormat,
        DatabaseError,
        NetworkError,
        AuthenticationFailed,
        NotImplemented,
        InvalidArgument,
        DeviceNotReady,
        DependencyMissing,
        OperationCancelled,
    };

    Code code{Unknown};
    QString message;

    Error() = default;
    Error(Code c, QString m) : code(c), message(std::move(m)) {}
};

/// Lightweight Result<T, Error> alternative to exceptions.
/// Use as: `Result<int> r = doThing(); if (!r) return r.error();`
template<typename T>
class Result {
public:
    Result(T value) : m_data(std::move(value)) {}
    Result(Error err) : m_data(std::move(err)) {}

    static Result<T> ok(T value) { return Result(std::move(value)); }
    static Result<T> err(Error::Code code, QString msg) {
        return Result(Error{code, std::move(msg)});
    }

    bool isOk() const { return std::holds_alternative<T>(m_data); }
    bool isErr() const { return std::holds_alternative<Error>(m_data); }
    explicit operator bool() const { return isOk(); }

    const T& value() const& { return std::get<T>(m_data); }
    T&& value() && { return std::move(std::get<T>(m_data)); }
    const Error& error() const& { return std::get<Error>(m_data); }

private:
    std::variant<T, Error> m_data;
};

/// Specialization for void-returning operations.
template<>
class Result<void> {
public:
    Result() : m_error(std::nullopt) {}
    Result(Error err) : m_error(std::move(err)) {}

    static Result<void> ok() { return Result(); }
    static Result<void> err(Error::Code code, QString msg) {
        return Result(Error{code, std::move(msg)});
    }

    bool isOk() const { return !m_error.has_value(); }
    bool isErr() const { return m_error.has_value(); }
    explicit operator bool() const { return isOk(); }

    const Error& error() const { return *m_error; }

private:
    std::optional<Error> m_error;
};

} // namespace soundshelf
