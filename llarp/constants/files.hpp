#pragma once
#include "platform.hpp"

#include <filesystem>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

namespace llarp
{
    constexpr auto our_rc_filename = "self.signed";
    constexpr auto our_identity_filename = "identity.key";
    constexpr auto our_enc_key_filename = "encryption.key";
    constexpr auto our_transport_key_filename = "transport.key";

    inline const std::filesystem::path nodedb_dirname{"nodedb"};
    inline const std::filesystem::path default_bootstrap{"bootstrap.signed"};

    inline std::filesystem::path GetDefaultDataDir()
    {
#ifndef _WIN32
        std::filesystem::path datadir{"/var/lib/lokinet"};
        if (auto uid = geteuid())
        {
            if (auto* pw = getpwuid(uid))
            {
                datadir = std::filesystem::path{pw->pw_dir} / ".lokinet";
            }
        }
        return datadir;
#else
        return std::filesystem::path{"C:\\ProgramData\\Lokinet"};
#endif
    }

    inline std::filesystem::path GetDefaultConfigFilename() { return "lokinet.ini"; }

    inline std::filesystem::path GetDefaultConfigPath() { return GetDefaultDataDir() / GetDefaultConfigFilename(); }

    inline std::filesystem::path GetDefaultBootstrap() { return GetDefaultDataDir() / default_bootstrap; }

}  // namespace llarp
