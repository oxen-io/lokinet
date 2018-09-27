#include <getopt.h>
#include <llarp.h>
#include <signal.h>
#include "logger.hpp"

#include <llarp/router_contact.hpp>
#include <llarp/time.h>

#include <fstream>
#include "buffer.hpp"
#include "crypto.hpp"
#include "fs.hpp"
#include "llarp/net.hpp"
#include "router.hpp"

#include <llarp/messages/dht.hpp>
//#include <llarp/dht/messages/findintro.hpp>
//#include <llarp/routing_endpoint.hpp>
//#include <llarp/crypt.hpp>  // for llarp::pubkey

struct llarp_main *ctx = 0;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

#ifndef TESTNET
#define TESTNET 0
#endif

void
displayRC(const llarp::RouterContact &rc)
{
  std::cout << rc.pubkey << std::endl;
  for(const auto &addr : rc.addrs)
  {
    std::cout << "AddressInfo: " << addr << std::endl;
  }
}

// fwd declr
struct check_online_request;

void
HandleDHTLocate(llarp_router_lookup_job *job)
{
  llarp::LogInfo("DHT result: ", job->found ? "found" : "not found");
  if(job->found)
  {
    // save to nodedb?
    displayRC(job->result);
  }
  // shutdown router

  // well because we're in the gotroutermessage, we can't sigint because we'll
  // deadlock because we're session locked
  // llarp_main_signal(ctx, SIGINT);

  // llarp_timer_run(logic->timer, logic->thread);
  // we'll we don't want logic thread
  // but we want to switch back to the main thread
  // llarp_logic_stop();
  // still need to exit this logic thread...
  llarp_main_abort(ctx);
}

int
main(int argc, char *argv[])
{
  // take -c to set location of daemon.ini
  // take -o to set log level
  // --generate-blank /path/to/file.signed
  // --update-ifs /path/to/file.signed
  // --key /path/to/long_term_identity.key
  // --import
  // --export

  // --generate /path/to/file.signed
  // --update /path/to/file.signed
  // --verify /path/to/file.signed
  // printf("has [%d]options\n", argc);
  if(argc < 2)
  {
    printf(
        "please specify: \n"
        "--generate  with a path to a router contact file\n"
        "--update    with a path to a router contact file\n"
        "--list      path to nodedb skiplist\n"
        "--import    with a path to a router contact file\n"
        "--export    a hex formatted public key\n"
        "--locate    a hex formatted public key\n"
        "--find      a base32 formatted service address\n"
        "--b32       a hex formatted public key\n"
        "--hex       a base32 formatted public key\n"
        "--localInfo \n"
        "--read      with a path to a router contact file\n"
        "--verify    with a path to a router contact file\n"
        "\n");
    return 0;
  }
  bool haveRequiredOptions = false;
  bool genMode             = false;
  bool updMode             = false;
  bool listMode            = false;
  bool importMode          = false;
  bool exportMode          = false;
  bool locateMode          = false;
  bool findMode            = false;
  bool localMode           = false;
  bool verifyMode          = false;
  bool readMode            = false;
  bool toHexMode           = false;
  bool toB32Mode           = false;
  int c;
  char *conffname;
  char defaultConfName[] = "daemon.ini";
  conffname              = defaultConfName;
  char *rcfname          = nullptr;
  char *nodesdir         = nullptr;

  llarp::RouterContact rc;
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
      case 'q':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname    = optarg;
        locateMode = true;
        break;
      case 'F':
        rcfname             = optarg;
        haveRequiredOptions = true;
        findMode            = true;
        break;
      case 'g':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname = optarg;
        genMode = true;
        break;
      case 'u':
        // printf ("option -u with value `%s'\n", optarg);
        rcfname = optarg;
        updMode = true;
        break;
      case 'n':
        localMode = true;
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
#undef MIN
  if(!haveRequiredOptions)
  {
    llarp::LogError("Parameters dont all have their required parameters.\n");
    return 0;
  }
  // printf("parsed options\n");

  if(!genMode && !updMode && !listMode && !importMode && !exportMode
     && !locateMode && !localMode && !readMode && !findMode && !toB32Mode
     && !toHexMode && !verifyMode)
  {
    llarp::LogError(
        "I don't know what to do, no generate or update parameter\n");
    return 1;
  }

  llarp::RouterContact tmp;

  if(verifyMode)
  {
    llarp_crypto crypto;
    llarp_crypto_init(&crypto);
    if(!rc.Read(rcfname))
    {
      std::cout << "failed to read " << rcfname << std::endl;
      return 1;
    }
    if(!rc.VerifySignature(&crypto))
    {
      std::cout << rcfname << " has invalid signature" << std::endl;
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
    return 0;
  }

  ctx = llarp_main_init(conffname, !TESTNET);
  if(!ctx)
  {
    llarp::LogError("Cant set up context");
    return 1;
  }
  signal(SIGINT, handle_signal);

  // is this Neuro or Jeff's?
  // this is the only one...
  if(listMode)
  {
    llarp_crypto crypto;
    llarp_crypto_init(&crypto);
    auto nodedb = llarp_nodedb_new(&crypto);
    llarp_nodedb_iter itr;
    itr.visit = [](llarp_nodedb_iter *i) -> bool {
      std::cout << i->rc->pubkey << std::endl;
      return true;
    };
    if(llarp_nodedb_load_dir(nodedb, nodesdir) > 0)
      llarp_nodedb_iterate_all(nodedb, itr);
    llarp_nodedb_free(&nodedb);
    return 0;
  }

  if(importMode)
  {
    if(rcfname == nullptr)
    {
      std::cout << "no file to import" << std::endl;
      return 1;
    }
    llarp_crypto crypto;
    llarp_crypto_init(&crypto);
    auto nodedb = llarp_nodedb_new(&crypto);
    if(!llarp_nodedb_ensure_dir(nodesdir))
    {
      std::cout << "failed to ensure " << nodesdir << strerror(errno)
                << std::endl;
      return 1;
    }
    llarp_nodedb_set_dir(nodedb, nodesdir);
    if(!rc.Read(rcfname))
    {
      std::cout << "failed to read " << rcfname << " " << strerror(errno)
                << std::endl;
      return 1;
    }

    if(!rc.VerifySignature(&crypto))
    {
      std::cout << rcfname << " has invalid signature" << std::endl;
      return 1;
    }

    if(!llarp_nodedb_put_rc(nodedb, rc))
    {
      std::cout << "failed to store " << strerror(errno) << std::endl;
      return 1;
    }

    std::cout << "imported " << rc.pubkey << std::endl;

    return 0;
  }

  if(genMode)
  {
    printf("Creating [%s]\n", rcfname);
    // if we zero it out then
    // set updated timestamp
    rc.last_updated = llarp_time_now_ms();
    // load longterm identity
    llarp_crypto crypt;
    llarp_crypto_init(&crypt);

    // which is in daemon.ini config: router.encryption-privkey (defaults
    // "encryption.key")
    fs::path encryption_keyfile = "encryption.key";
    llarp::SecretKey encryption;

    llarp_findOrCreateEncryption(&crypt, encryption_keyfile.string().c_str(),
                                 encryption);

    rc.enckey = llarp::seckey_topublic(encryption);

    // get identity public sig key
    fs::path ident_keyfile = "identity.key";
    llarp::SecretKey identity;
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.string().c_str(),
                               identity);

    rc.pubkey = llarp::seckey_topublic(identity);

    // this causes a segfault
    if(!rc.Sign(&crypt, identity))
    {
      std::cout << "failed to sign" << std::endl;
      return 1;
    }
    // set filename
    fs::path our_rc_file = rcfname;
    // write file
    rc.Write(our_rc_file.string().c_str());

    // llarp_rc_write(&tmp, our_rc_file.string().c_str());

    // release memory for tmp lists
    // llarp_rc_free(&tmp);
  }
  if(updMode)
  {
    printf("rcutil.cpp - Loading [%s]\n", rcfname);
    llarp::RouterContact tmp;
    // llarp_rc_clear(&rc);
    rc.Clear();
    // FIXME: new rc api
    // llarp_rc_read(rcfname, &rc);

    // set updated timestamp
    rc.last_updated = llarp_time_now_ms();
    // load longterm identity
    llarp_crypto crypt;

    // no longer used?
    // llarp_crypto_libsodium_init(&crypt);
    llarp::SecretKey identityKey;  // FIXME: Jeff requests we use this
    fs::path ident_keyfile = "identity.key";
    byte_t identity[SECKEYSIZE];
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.string().c_str(),
                               identity);

    // FIXME: update RC API
    // get identity public key
    // const uint8_t *pubkey = llarp::seckey_topublic(identity);

    // FIXME: update RC API
    // llarp_rc_set_pubsigkey(&rc, pubkey);
    // // FIXME: update RC API
    // llarp_rc_sign(&crypt, identity, &rc);

    // set filename
    fs::path our_rc_file_out = "update_debug.rc";
    // write file
    // FIXME: update RC API
    // rc.Write(our_rc_file.string().c_str());
    // llarp_rc_write(&tmp, our_rc_file_out.string().c_str());
  }

  if(listMode)
  {
    llarp_crypto crypto;
    // no longer used?
    // llarp_crypto_libsodium_init(&crypto);
    llarp_crypto_init(&crypto);
    auto nodedb = llarp_nodedb_new(&crypto);
    llarp_nodedb_iter itr;
    itr.visit = [](llarp_nodedb_iter *i) -> bool {
      std::cout << llarp::PubKey(i->rc->pubkey) << std::endl;
      return true;
    };
    if(llarp_nodedb_load_dir(nodedb, nodesdir) > 0)
      llarp_nodedb_iterate_all(nodedb, itr);
    llarp_nodedb_free(&nodedb);
    return 0;
  }
  if(exportMode)
  {
    llarp_main_loadDatabase(ctx);
    // llarp::LogInfo("Looking for string: ", rcfname);

    llarp::PubKey binaryPK;
    llarp::HexDecode(rcfname, binaryPK.data());

    llarp::LogInfo("Looking for binary: ", binaryPK);
    llarp::RouterContact *rc = llarp_main_getDatabase(ctx, binaryPK.data());
    if(!rc)
    {
      llarp::LogError("Can't load RC from database");
    }
    std::string filename(rcfname);
    filename.append(".signed");
    llarp::LogInfo("Writing out: ", filename);
    // FIXME: update RC API
    // rc.Write(our_rc_file.string().c_str());
    // llarp_rc_write(rc, filename.c_str());
  }
  if(locateMode)
  {
    llarp::LogInfo("Going online");
    llarp_main_setup(ctx);

    llarp::PubKey binaryPK;
    llarp::HexDecode(rcfname, binaryPK.data());

    llarp::LogInfo("Queueing job");
    llarp_router_lookup_job *job = new llarp_router_lookup_job;
    job->iterative               = true;
    job->found                   = false;
    job->hook                    = &HandleDHTLocate;
    // llarp_rc_new(&job->result);
    memcpy(job->target, binaryPK, PUBKEYSIZE);  // set job's target

    // create query DHT request
    check_online_request *request = new check_online_request;
    request->ptr                  = ctx;
    request->job                  = job;
    request->online               = false;
    request->nodes                = 0;
    request->first                = false;
    llarp_main_queryDHT(request);

    llarp::LogInfo("Processing");
    // run system and wait
    llarp_main_run(ctx);
  }
  if(findMode)
  {
    llarp::LogInfo("Going online");
    llarp_main_setup(ctx);

    llarp::LogInfo("Please find ", rcfname);
    std::string str(rcfname);

    llarp::service::Tag tag(rcfname);
    llarp::LogInfo("Tag ", tag);

    llarp::service::Address addr;
    str = str.append(".loki");
    llarp::LogInfo("Prestring ", str);
    bool res = addr.FromString(str.c_str());
    llarp::LogInfo(res ? "Success" : "not a base32 string");

    // Base32Decode(rcfname, addr);
    llarp::LogInfo("Addr ", addr);
    llarp::routing::DHTMessage *msg = new llarp::routing::DHTMessage();
    // uint64_t txid, const llarp::service::Address& addr
    // FIXME: new API?
    // msg->M.push_back(new llarp::dht::FindIntroMessage(tag, 1));

    // I guess we may need a router to get any replies
    llarp::LogInfo("Processing");
    // run system and wait
    llarp_main_run(ctx);
  }
  if(localMode)
  {
    // FIXME: update llarp_main_getLocalRC
    // llarp::RouterContact *rc = llarp_main_getLocalRC(ctx);
    // displayRC(rc);
    // delete it
  }
  {
    if(rc.Read(rcfname))
      displayRC(rc);
  }

  if(toB32Mode)
  {
    llarp::LogInfo("Converting hex string ", rcfname);
    std::string str(rcfname);

    llarp::PubKey binaryPK;
    // llarp::service::Address::FromString
    llarp::HexDecode(rcfname, binaryPK.data());
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
  // it's a unique_ptr, should clean up itself
  // llarp_main_free(ctx);
  return 0;  // success
}
