#include "file.hpp"

#include "formattable.hpp"
#include "logging.hpp"

#include <fcntl.h>

#include <fstream>
#include <ios>
#include <stdexcept>
#include <system_error>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace llarp::util
{
    static auto logcat = log::Cat("util.file");

    std::string file_to_string(const fs::path& filename, size_t max_size)
    {
        std::ifstream in;
        std::string contents;

        in.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        in.open(filename, std::ios::binary | std::ios::in);
        in.seekg(0, std::ios::end);
        auto size = in.tellg();
        in.seekg(0, std::ios::beg);

        if (auto sz = static_cast<size_t>(size); sz > max_size)
            throw std::length_error{
                "Cannot load {}: file size {} exceeds max allowed size {}"_format(filename, sz, max_size)};

        contents.resize(size);
        in.read(contents.data(), size);
        return contents;
    }

    void buffer_to_file(const fs::path& filename, std::string_view contents)
    {
        std::ofstream out;
        out.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        out.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    }

    static std::error_code errno_error()
    {
        int e = errno;
        errno = 0;
        return std::make_error_code(static_cast<std::errc>(e));
    }

    error_code_t EnsurePrivateFile(fs::path pathname)
    {
        errno = 0;
        error_code_t ec = errno_error();
        const auto str = pathname.string();
        if (fs::exists(pathname, ec))  // file exists
        {
            fs::permissions(pathname, fs::perms::owner_read | fs::perms::owner_write, ec);
            if (ec)
                log::error(logcat, "failed to set permissions on {}", pathname);
        }
        else  // file is not there
        {
            errno = 0;
            int fd = ::open(str.c_str(), O_RDWR | O_CREAT, 0600);
            ec = errno_error();
            if (fd != -1)
            {
                ::close(fd);
            }
        }

#ifndef WIN32
        if (ec)
            log::error(logcat, "failed to ensure {}: {}", str, ec.message());
        return ec;
#else
        return {};
#endif
    }

}  // namespace llarp::util
