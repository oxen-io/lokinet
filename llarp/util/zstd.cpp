#include "zstd.hpp"

#include "formattable.hpp"

#include <zstd.h>

#include <cassert>
#include <stdexcept>

namespace llarp::zstd
{

    static ZSTD_CCtx* cctx(void* c) { return static_cast<ZSTD_CCtx*>(c); }
    static ZSTD_DCtx* dctx(void* d) { return static_cast<ZSTD_DCtx*>(d); }

    compressor::compressor() : _context{ZSTD_createCCtx()} {}
    compressor::~compressor() { ZSTD_freeCCtx(cctx(_context)); }

    decompressor::decompressor() : _context{ZSTD_createDCtx()} {}
    decompressor::~decompressor() { ZSTD_freeDCtx(dctx(_context)); }

    std::vector<std::byte> compressor::compress(
        std::span<const std::byte> data, int level, std::span<const std::byte> prefix)
    {
        auto* ctx = cctx(_context);
        std::vector<std::byte> compressed;
        compressed.resize(prefix.size() + ZSTD_compressBound(data.size()));
        if (!prefix.empty())
            std::memcpy(compressed.data(), prefix.data(), prefix.size());
        auto size = ZSTD_compressCCtx(
            ctx, compressed.data() + prefix.size(), compressed.size() - prefix.size(), data.data(), data.size(), level);
        if (ZSTD_isError(size))
            throw std::runtime_error{"Compression failed: {}"_format(ZSTD_getErrorName(size))};
        compressed.resize(prefix.size() + size);
        return compressed;
    }
    std::vector<std::byte> compressor::compress(std::string_view data, int level, std::string_view prefix)
    {
        return compress(
            {reinterpret_cast<const std::byte*>(data.data()), data.size()},
            level,
            {reinterpret_cast<const std::byte*>(prefix.data()), prefix.size()});
    }

    template <typename B>
    static std::vector<std::byte> compress_piecewise(ZSTD_CCtx* ctx, std::span<const B> buffers, int level, const B prefix)
    {
        ZSTD_CCtx_reset(ctx, ZSTD_reset_session_and_parameters);
        size_t in_size = 0;
        for (auto& b : buffers)
            in_size += b.size();
        ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, level);
        ZSTD_CCtx_setPledgedSrcSize(ctx, in_size);
        std::vector<std::byte> compressed;
        compressed.resize(prefix.size() + ZSTD_compressBound(in_size));
        if (!prefix.empty())
            std::memcpy(compressed.data(), prefix.data(), prefix.size());
        ZSTD_outBuffer output{.dst = compressed.data() + prefix.size(), .size = compressed.size() - prefix.size(), .pos = 0};
        for (const auto& buf : buffers)
        {
            const auto last = &buf == &buffers.back();
            const auto mode = last ? ZSTD_e_end : ZSTD_e_continue;
            ZSTD_inBuffer input{.src = buf.data(), .size = buf.size(), .pos = 0};
            bool finished;
            do
            {
                const auto remaining = ZSTD_compressStream2(ctx, &output, &input, mode);
                if (ZSTD_isError(remaining))
                    throw std::runtime_error{"Compression failed: {}"_format(ZSTD_getErrorName(remaining))};
                finished = last ? remaining == 0 : input.pos == input.size;
            } while (not finished);
            assert(input.pos == input.size);
        }
        assert(prefix.size() + output.pos <= compressed.size());
        compressed.resize(prefix.size() + output.pos);
        return compressed;
    }

    std::vector<std::byte> compressor::compress(std::span<const std::string_view> buffers, int level, std::string_view prefix)
    {
        return compress_piecewise(cctx(_context), buffers, level, prefix);
    }
    std::vector<std::byte> compressor::compress(std::span<const std::span<const std::byte>> buffers, int level, const std::span<const std::byte> prefix)
    {
        return compress_piecewise(cctx(_context), buffers, level, prefix);
    }

    /// Attempts to decompress `data`.  Returns nullopt if decompression fails.  If max_size is
    /// non-zero then this returns nullopt if the decompressed data would expand to more than
    /// max_size bytes.
    std::optional<std::vector<std::byte>> decompressor::decompress(
        std::span<const std::byte> compressed, size_t max_size)
    {
        auto* ctx = dctx(_context);
        ZSTD_initDStream(ctx);
        ZSTD_inBuffer input{.src = compressed.data(), .size = compressed.size(), .pos = 0};

        std::array<std::byte, 16384> out_buf;
        ZSTD_outBuffer output{.dst = out_buf.data(), .size = out_buf.size(), .pos = 0};

        std::vector<std::byte> decompressed;

        size_t ret;
        do
        {
            output.pos = 0;
            ret = ZSTD_decompressStream(ctx, &output, &input);
            if (ZSTD_isError(ret) or (max_size > 0 && decompressed.size() + output.pos > max_size))
                return std::nullopt;

            decompressed.insert(decompressed.end(), out_buf.begin(), out_buf.begin() + output.pos);
        } while (ret > 0 or input.pos < input.size);

        return decompressed;
    }

}  // namespace llarp::zstd
