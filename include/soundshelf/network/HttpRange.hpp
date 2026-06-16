#pragma once

#include <QByteArray>

namespace soundshelf {

/**
 * @brief Both-bounds-inclusive byte range within a file.
 */
struct ByteRange {
    qint64 start = 0; ///< First byte offset (inclusive, 0-based).
    qint64 end   = 0; ///< Last byte offset (inclusive).

    /// @brief Returns the number of bytes covered by this range.
    [[nodiscard]] qint64 length() const { return end - start + 1; }
};

/**
 * @brief Result of parsing a Range request header.
 */
enum class RangeStatus {
    None,          ///< No Range header present; caller should serve a full 200 response.
    Satisfiable,   ///< Valid single range that lies within the file; serve 206.
    Unsatisfiable, ///< Range start >= file size or file is empty; serve 416.
    Malformed,     ///< Syntax error, unsupported unit, or multi-range; RFC 7233 fallback to 200.
};

/**
 * @brief Combined outcome of HttpRange::parse().
 */
struct RangeResult {
    RangeStatus status = RangeStatus::None;
    ByteRange   range  = {}; ///< Valid only when status == RangeStatus::Satisfiable.
};

/**
 * @brief Pure helpers for RFC 7233 HTTP Range request processing.
 *
 * Only single byte ranges with the @c bytes unit are supported.
 * Multi-range requests (comma-separated specs) are treated as Malformed —
 * RFC 7233 §3.1 permits servers to ignore a Range header when it is not useful.
 *
 * This class has no dependency on QHttpServer and compiles unconditionally,
 * making it fully unit-testable on builds that lack SOUNDSHELF_HAVE_HTTPSERVER.
 */
class HttpRange {
public:
    /**
     * @brief Parses the raw value of the @c Range request header.
     *
     * @param headerValue  Raw header value, e.g. @c "bytes=0-499".
     *                     An empty value yields RangeStatus::None.
     * @param totalSize    Total file size in bytes.
     * @return RangeResult with resolved status and, when Satisfiable, the clamped range.
     *
     * Supported forms:
     * - @c bytes=\<start\>-\<end\>  — explicit range; end is clamped to totalSize-1
     * - @c bytes=\<start\>-         — open end; resolves to \<start\>…totalSize-1
     * - @c bytes=-\<suffix\>        — last N bytes; start clamped to 0 when suffix >= totalSize
     *
     * Logs malformed input at @c soundshelf.network.http debug level.
     */
    static RangeResult parse(const QByteArray& headerValue, qint64 totalSize);

    /**
     * @brief Formats a @c Content-Range header value for a satisfied (206) response.
     *
     * @param r          The satisfiable byte range.
     * @param totalSize  Total file size in bytes.
     * @return  e.g. @c "bytes 0-499/1234"
     */
    static QByteArray contentRange(const ByteRange& r, qint64 totalSize);

    /**
     * @brief Formats a @c Content-Range header value for a 416 RangeNotSatisfiable response.
     *
     * @param totalSize  Total file size in bytes.
     * @return  e.g. @c "bytes *\/1234" (the wildcard total length form)
     */
    static QByteArray unsatisfiedContentRange(qint64 totalSize);
};

} // namespace soundshelf
