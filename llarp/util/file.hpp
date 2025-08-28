#pragma once

#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#ifndef _MSC_VER
#include <dirent.h>
#endif

#include <oxenc/common.h>

namespace llarp::util
{
    /// Reads a binary file from disk into a string.  Throws on error.
    std::string file_to_string(const std::filesystem::path& filename, size_t max_size = std::numeric_limits<size_t>::max());

    /// Reads a binary file from disk directly into a buffer.  Throws a std::length_error if the
    /// file is bigger than the buffer.  Returns the bytes copied on success.
    size_t file_to_buffer(const std::filesystem::path& filename, char* buffer, size_t buffer_size);

    /// Dumps binary string contents to disk. The file is overwritten if it already exists.  Throws
    /// on error.
    void buffer_to_file(const std::filesystem::path& filename, std::string_view contents);

    struct FileHash
    {
        size_t operator()(const std::filesystem::path& f) const
        {
            std::hash<std::string> h;
            return h(f.string());
        }
    };

    using error_code_t = std::error_code;

    /// Ensure that a file exists and has correct permissions
    /// return any error code or success
    error_code_t EnsurePrivateFile(const std::filesystem::path& pathname);

    /// open a stream to a file and ensure it exists before open
    /// sets any permissions on creation
    template <typename T>
    std::optional<T> OpenFileStream(const std::filesystem::path& pathname, std::ios::openmode mode)
    {
        if (EnsurePrivateFile(pathname))
            return {};
        return std::make_optional<T>(pathname, mode);
    }

}  // namespace llarp::util
