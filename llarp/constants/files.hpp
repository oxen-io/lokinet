#pragma once
#include "platform.hpp"

#include <filesystem>

#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace llarp
{
    constexpr auto our_rc_filename = "self.signed";
    constexpr auto our_identity_filename = "identity.key";
    constexpr auto our_enc_key_filename = "encryption.key";
    constexpr auto our_transport_key_filename = "transport.key";

    inline const fs::path nodedb_dirname{"nodedb"};
    inline const fs::path default_bootstrap{"bootstrap.signed"};

    inline fs::path GetDefaultDataDir()
    {
#ifndef _WIN32
        fs::path datadir{"/var/lib/lokinet"};
        if (auto uid = geteuid())
        {
            if (auto* pw = getpwuid(uid))
            {
                datadir = fs::path{pw->pw_dir} / ".lokinet";
            }
        }
        return datadir;
#else
        return fs::path{"C:\\ProgramData\\Lokinet"};
#endif
    }

    inline fs::path GetDefaultConfigFilename() { return "lokinet.ini"; }

    inline fs::path GetDefaultConfigPath() { return GetDefaultDataDir() / GetDefaultConfigFilename(); }

    inline fs::path GetDefaultBootstrap() { return GetDefaultDataDir() / default_bootstrap; }

}  // namespace llarp
