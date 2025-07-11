#pragma once

#include "auth.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace llarp
{
    class Router;
}
namespace llarp::auth
{
    /// how to interpret an file for auth
    enum class AuthFileType
    {
        PLAIN,
        HASHES,
    };

    struct FileAuthPolicy final : public AuthPolicy
    {
        FileAuthPolicy(Router& r, std::vector<fs::path> files, AuthFileType filetype)
            : AuthPolicy{r}, _files{std::move(files)}, _type{filetype}
        {}

      private:
        const std::vector<fs::path> _files;
        const AuthFileType _type;
        mutable util::Mutex _m;
        std::unordered_set<session_tag> _pending;
        /// returns an auth result for a auth info challange, opens every file until it finds a
        /// token matching it this is expected to be done in the IO thread
        AuthResult check_files(const AuthInfo& info) const;

        bool check_passwd(std::string hash, std::string challenge) const;
    };

}  // namespace llarp::auth
