#pragma once

#include <util/fs.hpp>

#include <stdlib.h>

#ifndef _WIN32
#include <pwd.h>
#endif

namespace llarp
{
  constexpr auto our_rc_filename = "self.signed";
  constexpr auto our_identity_filename = "identity.key";
  constexpr auto our_enc_key_filename = "encryption.key";
  constexpr auto our_transport_key_filename = "transport.key";

  constexpr auto nodedb_dirname = "nodedb";

  inline fs::path
  GetDefaultDataDir()
  {
#ifdef _WIN32
    const fs::path homedir = getenv("APPDATA");
    return homedir / "lokinet";
#else
    fs::path homedir;

    auto pw = getpwuid(getuid());
    if ((pw and pw->pw_uid) and pw->pw_dir)
    {
      homedir = pw->pw_dir;
    }
    else
    {
      // no home dir specified yea idk
      homedir = "/var/lib/lokinet";
      return homedir;
    }
    return homedir / ".lokinet";
#endif
  }

  inline fs::path
  GetDefaultConfigFilename()
  {
    return "lokinet.ini";
  }

  inline fs::path
  GetDefaultConfigPath()
  {
    return GetDefaultDataDir() / GetDefaultConfigFilename();
  }

}  // namespace llarp
