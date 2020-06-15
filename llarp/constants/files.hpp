#pragma once

#include <util/fs.hpp>

#include <stdlib.h>

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
#else
    const fs::path homedir = getenv("HOME");
#endif
    return homedir / ".lokinet";
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
