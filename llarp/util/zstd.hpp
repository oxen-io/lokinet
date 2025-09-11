#pragma once
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace llarp::zstd
{
    // Compression class that can be held to be used repeatedly, saving some system resources for
    // repeated compressions.  NOT thread-safe (i.e. the same instance must not be used from
    // different threads at the same time).
    class compressor
    {
      private:
        void* _context;

      public:
        static constexpr int DEFAULT_LEVEL = 3;

        compressor();

        compressor(compressor&&) = delete;
        compressor(const compressor&) = delete;
        compressor& operator=(const compressor&) = delete;
        compressor& operator=(compressor&&) = delete;

        // Compress a single buffer.  Throws on serious error.  If non-empty, `prefix` will be
        // prepended to the returned vector.
        std::vector<std::byte> compress(
            std::span<const std::byte> data, int level = DEFAULT_LEVEL, std::span<const std::byte> prefix = {});
        std::vector<std::byte> compress(std::string_view data, int level = DEFAULT_LEVEL, std::string_view prefix = {});

        // Compress the concatenation of multiple buffers without requiring pre-concatenation.
        // Throws on serious error.
        std::vector<std::byte> compress(
            std::span<const std::span<const std::byte>> buffers,
            int level = DEFAULT_LEVEL,
            std::span<const std::byte> prefix = {});
        std::vector<std::byte> compress(
            std::span<const std::string_view> buffers, int level = DEFAULT_LEVEL, std::string_view prefix = {});

        ~compressor();
    };

    // Decompression class that can be stored to save initialization if used repeatedly.  NOT
    // thread-safe.
    class decompressor
    {
      private:
        void* _context;

      public:
        decompressor();

        decompressor(decompressor&&) = delete;
        decompressor(const decompressor&) = delete;
        decompressor& operator=(const decompressor&) = delete;
        decompressor& operator=(decompressor&&) = delete;

        // Attempts to decompress a single buffer with maximum allowed decompressed size of
        // `max_size` (if non-zero).  Returns nullptr if compression failed (or if the `max_size`
        // limit is hit).
        std::optional<std::vector<std::byte>> decompress(std::span<const std::byte> compressed, size_t max_size = 0);

        ~decompressor();
    };

}  // namespace llarp::zstd
