#include <util/buffer.hpp>
#include <crypto/crypto.hpp>
#include <crypto/crypto_libsodium.hpp>
#include <util/fs.hpp>
#include <llarp.h>
#include <util/logger.hpp>
#include <messages/dht.hpp>
#include <net/net.hpp>
#include <nodedb.hpp>
#include <router/router.hpp>
#include <router_contact.hpp>
#include <util/time.hpp>
#include <util/types.hpp> // for crytpo
#include <llarp.hpp> // for crytpo

#include <fstream>
#include <getopt.h>
#include <signal.h>

#ifndef TESTNET
#define TESTNET 0
#endif

void
displayRC(const llarp::RouterContact &rc)
{
  //std::cout << rc.pubkey << std::endl;
  std::cout << "router identity and dht routing key: " << rc.pubkey
  << std::endl;
  
  std::cout << "router encryption key: " << rc.enckey << std::endl;
  
  if(rc.HasNick())
    std::cout << "router nickname: " << rc.Nick() << std::endl;
  
  std::cout << "advertised addresses: " << std::endl;
  for(const auto &addr : rc.addrs)
  {
    std::cout << addr << std::endl;
  }
  std::cout << std::endl;
  
  std::cout << "advertised exits: ";
  if(rc.exits.size())
  {
    for(const auto &exit : rc.exits)
    {
      std::cout << exit << std::endl;
    }
  }
  else
  {
    std::cout << "none";
  }
  std::cout << std::endl;
  
}

int
main(int argc, char *argv[])
{
  // general options
  // take -c to set location of daemon.ini
  // take -o to set log level
  // force or overwrite option in case RC already exists (on creation)
  // --ekey /path/to/long_term_encryption.key
  // --ikey /path/to/long_term_identity.key

  // util tops
    // --localInfo
    // --b32
    // --hex
  // rc file ops
    // --generate-blank /path/to/file.signed
    // --generate /path/to/file.signed
    // --update-ifs /path/to/file.signed
    // --update /path/to/file.signed
    // --verify /path/to/file.signed
  // nodedb ops
    // --import
    // --export
  // printf("has [%d]options\n", argc);
  if(argc < 2)
  {
    printf(
        "please specify: \n"
        "--localInfo \n"
        "--list      path to nodedb skiplist\n"
        "--generate  with a path to a router contact file\n"
        "--read      with a path to a router contact file\n"
        "--verify    with a path to a router contact file\n"
        "--update    with a path to a router contact file\n"
        "--import    with a path to a router contact file\n"
        "--export    a hex formatted public key\n"
        "--b32       a hex formatted public key\n"
        "--hex       a base32 formatted public key\n"
        "\n");
    return 0;
  }
  bool haveRequiredOptions = false;
  bool genMode             = false;
  bool updMode             = false;
  bool listMode            = false;
  bool importMode          = false;
  bool exportMode          = false;
  bool localMode           = false;
  bool verifyMode          = false;
  bool readMode            = false;
  bool toHexMode           = false;
  bool toB32Mode           = false;
  int c;
  char *conffname;
  char defaultConfName[] = "lokinet.ini";
  conffname              = defaultConfName;
  char *rcfname          = nullptr;
  char *nodesdir         = nullptr;

  while(1)
  {
    static struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"config", required_argument, 0, 'c'},
        {"logLevel", required_argument, 0, 'o'},
        {"generate", required_argument, 0, 'g'},
        {"update", required_argument, 0, 'u'},
        {"list", required_argument, 0, 'l'},
        {"import", required_argument, 0, 'i'},
        {"export", required_argument, 0, 'e'},
        {"locate", required_argument, 0, 'q'},
        {"find", required_argument, 0, 'F'},
        {"localInfo", no_argument, 0, 'n'},
        {"read", required_argument, 0, 'r'},
        {"b32", required_argument, 0, 'b'},
        {"hex", required_argument, 0, 'h'},
        {"verify", required_argument, 0, 'V'},
        {0, 0, 0, 0}};
    int option_index = 0;
    c = getopt_long(argc, argv, "c:f:o:g:lu:i:e:q:F:nr:b:h:V:", long_options,
                    &option_index);
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
    if(c == -1)
      break;
    switch(c)
    {
      case 0:
        break;
      case 'c':
        conffname = optarg;
        break;
      case 'o':
        if(strncmp(optarg, "debug",
                   MIN(strlen(optarg), static_cast< unsigned long >(5)))
           == 0)
        {
          llarp::SetLogLevel(llarp::eLogDebug);
        }
        else if(strncmp(optarg, "info",
                        MIN(strlen(optarg), static_cast< unsigned long >(4)))
                == 0)
        {
          llarp::SetLogLevel(llarp::eLogInfo);
        }
        else if(strncmp(optarg, "warn",
                        MIN(strlen(optarg), static_cast< unsigned long >(4)))
                == 0)
        {
          llarp::SetLogLevel(llarp::eLogWarn);
        }
        else if(strncmp(optarg, "error",
                        MIN(strlen(optarg), static_cast< unsigned long >(5)))
                == 0)
        {
          llarp::SetLogLevel(llarp::eLogError);
        }
        break;
      case 'V':
        rcfname             = optarg;
        haveRequiredOptions = true;
        verifyMode          = true;
        break;
      case 'f':
        rcfname = optarg;
        break;
      case 'l':
        nodesdir = optarg;
        listMode = true;
        haveRequiredOptions = true;
        break;
      case 'i':
        // printf ("option -g with value `%s'\n", optarg);
        nodesdir   = optarg;
        importMode = true;
        break;
      case 'e':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname    = optarg;
        exportMode = true;
        break;
      case 'g':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname = optarg;
        if (rcfname) {
          haveRequiredOptions = true;
        }
        genMode = true;
        break;
      case 'u':
        // printf ("option -u with value `%s'\n", optarg);
        rcfname = optarg;
        if (rcfname) {
          haveRequiredOptions = true;
        }
        updMode = true;
        break;
      case 'n':
        localMode = true;
        haveRequiredOptions = true;
        break;
      case 'r':
        rcfname  = optarg;
        readMode = true;
        break;
      case 'b':
        rcfname             = optarg;
        haveRequiredOptions = true;
        toB32Mode           = true;
        break;
      case 'h':
        rcfname             = optarg;
        haveRequiredOptions = true;
        toHexMode           = true;
        break;
      default:
        printf("Bad option: %c\n", c);
        return -1;
    }
  }
  if (importMode) {
    if (rcfname && nodesdir) {
      haveRequiredOptions = true;
    }
  }
#undef MIN
  if(!haveRequiredOptions)
  {
    llarp::LogError("Parameters dont all have their required parameters.\n");
    return 0;
  }
  // printf("parsed options\n");

  if(!genMode && !updMode && !listMode && !importMode && !exportMode
    && !localMode && !readMode && !toB32Mode
     && !toHexMode && !verifyMode)
  {
    llarp::LogError(
        "I don't know what to do, no generate or update parameter\n");
    return 1;
  }

#ifdef LOKINET_DEBUG
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
#endif

  llarp::RouterContact rc;

  // llarp::SetLogLevel(llarp::eLogError);
  std::unique_ptr< llarp::Context > n_ctx = std::make_unique< llarp::Context >();
  n_ctx->LoadConfig(conffname);
  // probably should just set up here..

  // restore log level
  // llarp::SetLogLevel();

  if(readMode)
  {
    n_ctx->Setup(); // will set up router, we can use Now()
    std::unique_ptr< llarp::Crypto > crypto = std::make_unique< llarp::sodium::CryptoLibSodium >();
    if(!rc.Read(rcfname))
    {
      std::cout << "failed to read " << rcfname << std::endl;
      return 1;
    }
    if(!rc.IsPublicRouter())
    {
      std::cout << rcfname << " is not a public router";
      if(rc.addrs.size() == 0)
      {
        std::cout << " because it has no public addresses";
      }
      std::cout << std::endl;
      return 1;
    }
    displayRC(rc);
    return 0;
  }
  if(verifyMode)
  {
    n_ctx->Setup(); // will set up router, we can use Now()
    std::unique_ptr< llarp::Crypto > crypto = std::make_unique< llarp::sodium::CryptoLibSodium >();
    if(!rc.Read(rcfname))
    {
      std::cout << "failed to read " << rcfname << std::endl;
      return 1;
    }
    llarp_time_t now = n_ctx->router->Now();
    if(!rc.Verify(crypto.get(), now))
    {
      std::cout << rcfname << " is invalid" << std::endl;
      return 1;
    }
    if(!rc.IsPublicRouter())
    {
      std::cout << rcfname << " is not a public router";
      if(rc.addrs.size() == 0)
      {
        std::cout << " because it has no public addresses";
      }
      std::cout << std::endl;
      return 1;
    }
    
    displayRC(rc);

    return 0;
  }
  if(listMode)
  {
    n_ctx->Setup(); // will load the nodedb
    //n_ctx->LoadDatabase(); // can't use this because diskworker isn't set up
    llarp_nodedb_iter itr;
    itr.visit = [](llarp_nodedb_iter *i) -> bool {
      // is going to output the key in hex
      std::cout << llarp::RouterID(i->rc->pubkey) << std::endl;
      return true;
    };
    if (n_ctx->nodedb->num_loaded() > 0) {
      std::cout << "has nodes" << std::endl;
      n_ctx->nodedb->iterate_all(itr);
    }
    return 0;
  }

  if(importMode)
  {
    if(rcfname == nullptr)
    {
      std::cout << "no file to import" << std::endl;
      return 1;
    }
    std::unique_ptr< llarp::Crypto > crypto = std::make_unique< llarp::sodium::CryptoLibSodium >();
    n_ctx->Setup(); // will load the nodedb
    if(!rc.Read(rcfname))
    {
      std::cout << "failed to read " << rcfname << " " << strerror(errno)
                << std::endl;
      return 1;
    }
    
    if(!rc.Verify(crypto.get(), n_ctx->router->Now()))
    {
      std::cout << rcfname << " is invalid" << std::endl;
      return 1;
    }
    if (n_ctx->nodedb->Insert(rc)) {
      std::cout << "failed to store " << strerror(errno) << std::endl;
      return 1;
    }
    std::cout << "imported " << rc.pubkey << std::endl;
    return 0;
  }
  if(genMode)
  {
    if(rcfname == nullptr)
    {
      std::cout << "no path to create" << std::endl;
      return 1;
    }
    printf("Creating [%s]\n", rcfname);

    // if we zero it out then
    //rc.Clear();
    // set updated timestamp
    rc.last_updated = llarp::time_now_ms();
    // load longterm identity
    std::unique_ptr< llarp::Crypto > crypto = std::make_unique< llarp::sodium::CryptoLibSodium >();
    //llarp::Crypto crypt(llarp::Crypto::sodium{});
    
    // which is in lokinet.ini config: router.encryption-privkey (defaults
    // "encryption.key")
    fs::path encryption_keyfile = "encryption.key";
    llarp::SecretKey encryption;

    llarp_findOrCreateEncryption(crypto.get(), encryption_keyfile, encryption);

    rc.enckey = llarp::seckey_topublic(encryption);

    // get identity public sig key
    fs::path ident_keyfile = "identity.key";
    llarp::SecretKey identity;
    llarp_findOrCreateIdentity(crypto.get(), ident_keyfile, identity);

    rc.pubkey = llarp::seckey_topublic(identity);

    if(!rc.Sign(crypto.get(), identity))
    {
      std::cout << "failed to sign" << std::endl;
      return 1;
    }
    // set filename
    fs::path our_rc_file = rcfname;
    // write file
    if (!rc.Write(our_rc_file.string().c_str())) {
      std::cout << "failed to write" << std::endl;
      return 1;
    }
    displayRC(rc);
  }
  if(updMode)
  {
    llarp::LogInfo("Loading ", rcfname);

    if (!rc.Read(rcfname)) {
      std::cout << "failed to read " << rcfname << " " << strerror(errno)
      << std::endl;
      return 1;
    }
    displayRC(rc);

    // set updated timestamp
    rc.last_updated = llarp::time_now_ms();
    llarp::LogInfo("Updating timestamp ", rc.last_updated);

    // load longterm identity
    std::unique_ptr< llarp::Crypto > crypto = std::make_unique< llarp::sodium::CryptoLibSodium >();

    fs::path ident_keyfile = "identity.key";
    llarp::SecretKey identity;
    llarp_findOrCreateIdentity(crypto.get(), ident_keyfile, identity);

    // set identity public key
    rc.pubkey = llarp::seckey_topublic(identity);

    // sign RC
    if(!rc.Sign(crypto.get(), identity))
    {
      std::cout << "failed to sign" << std::endl;
      return 1;
    }

    // set filename
    fs::path our_rc_file_out = "update_debug.rc";

    // write file
    llarp::LogInfo("Writing ", our_rc_file_out.string().c_str());
    if (!rc.Write(our_rc_file_out.string().c_str())) {
      std::cout << "failed to write " << our_rc_file_out << " " << strerror(errno)
      << std::endl;
      return 1;
    }
    displayRC(rc);
  }

  if(exportMode)
  {
    // UNTESTED
    //llarp_main_loadDatabase(ctx);
    n_ctx->Setup(); // will load the nodedb
    // llarp::LogInfo("Looking for string: ", rcfname);

    llarp::PubKey binaryPK;
    llarp::HexDecode(rcfname, binaryPK.data(), binaryPK.size());

    llarp::LogInfo("Looking for binary: ", binaryPK);
    //llarp::RouterContact *rc2 = llarp_main_getDatabase(ctx, binaryPK.data());
    if(!n_ctx->nodedb->Get(binaryPK.data(), rc))
    {
      llarp::LogError("Can't load RC from database");
    }
    std::string filename(rcfname);
    filename.append(".signed");
    llarp::LogInfo("Writing out: ", filename);
    rc.Write(filename.c_str());
    // FIXME: update RC API
    // rc.Write(our_rc_file.string().c_str());
    // llarp_rc_write(rc, filename.c_str());
  }
  if(localMode)
  {
    // FIXME:
    n_ctx->Setup(); // will load the nodedb
    // any way to get the self path?
    // it'll be 0
    //n_ctx->Run();
    rc = n_ctx->router->rc();
    displayRC(rc);
  }

  if(toB32Mode)
  {
    llarp::LogInfo("Converting hex string ", rcfname);
    std::string str(rcfname);

    llarp::PubKey binaryPK;
    // llarp::service::Address::FromString
    llarp::HexDecode(rcfname, binaryPK.data(), binaryPK.size());
    char tmp[(1 + 32) * 2] = {0};
    std::string b32        = llarp::Base32Encode(binaryPK, tmp);
    llarp::LogInfo("to base32 ", b32);
  }
  if(toHexMode)
  {
    llarp::service::Address addr;
    llarp::Base32Decode(rcfname, addr);
    llarp::LogInfo("Converting base32 string ", addr);

    // llarp::service::Address::ToString
    char ftmp[68] = {0};
    const char *hexname =
        llarp::HexEncode< llarp::service::Address, decltype(ftmp) >(addr, ftmp);

    llarp::LogInfo("to hex ", hexname);
  }

  return 0;  // success
}
