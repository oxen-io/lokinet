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
  // printf("has [%d]options\n", argc);
  if(argc < 2)
  {
    printf(
        "please specify: \n"
        "--locate    a hex formatted public key\n"
        "--find      a base32 formatted service address\n"
        "--b32       a hex formatted public key\n"
        "--hex       a base32 formatted public key\n"
        "--localInfo \n"
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
  char defaultConfName[] = "/Users/admin/.lokinet/lokinet-snappnet.ini";
  conffname              = defaultConfName;
  char *rcfname          = nullptr;

  llarp::RouterContact rc;
  while(1)
  {
    static struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"config", required_argument, 0, 'c'},
        {"logLevel", required_argument, 0, 'o'},
        {"locate", required_argument, 0, 'q'},
        {"find", required_argument, 0, 'F'},
        {"localInfo", no_argument, 0, 'n'},
        {"read", required_argument, 0, 'r'},
        {"b32", required_argument, 0, 'b'},
        {"hex", required_argument, 0, 'h'},
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
      case 'f':
        rcfname = optarg;
        break;
      case 'q':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname    = optarg;
        locateMode = true;
        haveRequiredOptions = true;
        break;
      case 'F':
        rcfname             = optarg;
        haveRequiredOptions = true;
        findMode            = true;
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

#ifdef LOKINET_DEBUG
  absl::SetMutexDeadlockDetectionMode(absl::OnDeadlockCycle::kAbort);
#endif

  llarp::RouterContact tmp;

  std::unique_ptr< llarp::Context > n_ctx = std::make_unique< llarp::Context >();
  if (!n_ctx->LoadConfig(conffname))
  {
    llarp::LogInfo("Can't load ", conffname);
    return;
  }

  if(locateMode)
  {
    llarp::LogInfo("Going online");
    n_ctx->Setup(); // will set up router, we can use Now()

    llarp::RouterID addr;
    std::string snodeAddrStr(rcfname);
    snodeAddrStr += ".snode";
    addr.FromString(snodeAddrStr);

    llarp::LogInfo("Queueing job for ", addr);
    /*
    llarp_router_lookup_job *job = new llarp_router_lookup_job;
    job->iterative               = true;
    job->found                   = false;
    job->hook                    = &HandleDHTLocate;
    // llarp_rc_new(&job->result);
    job->target = binaryPK;  // set job's target
     */

    // create query DHT request
    /*
    check_online_request *request = new check_online_request;
    request->ptr                  = ctx;
    request->job                  = job;
    request->online               = false;
    request->nodes                = 0;
    request->first                = false;
    */
    
    // make sure we have one endpoint
    llarp::Router *rtr = dynamic_cast<llarp::Router *>(n_ctx->router.get()); // cast it
    rtr->CreateDefaultHiddenService();
    
    n_ctx->LoadDatabase(); // make sure db is loaded
    rtr->_hiddenServiceContext.ForEachService(
                                     [=](const std::string &,
                                         const std::shared_ptr< llarp::service::Endpoint > &ep) -> bool {
                                       // will call LookupRouter which the GetEstablishedPathClosestTo will still be null
                                       ep->Start();
                                       ep->EnsurePathToSNode(addr, [=](const llarp::RouterID &addr, llarp::exit::BaseSession_ptr s) {
                                         llarp::LogInfo("std ", addr);
                                       });
                                       return true;
                                     });

    // we need a path built
    // and we can't tick yet
    // so we need a path future...
    /*
    n_ctx->router->LookupRouter(addr, [](const std::vector<llarp::RouterContact> &contacts){
      llarp::LogInfo("Found ", contacts.size());
      llarp::LogInfo("Found ", contacts.size());
      llarp::LogInfo("Found ", contacts.size());
    });
     */
    //llarp_main_queryDHT(request);

    llarp::LogInfo("Processing");
    // run system and wait
    n_ctx->Run();
  }
  if(findMode)
  {
    llarp::LogInfo("Going online");
    n_ctx->Setup(); // will set up router, we can use Now()

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

    // uint64_t txid, const llarp::service::Address& addr
    // FIXME: new API?
    // msg->M.push_back(new llarp::dht::FindIntroMessage(tag, 1));

    // I guess we may need a router to get any replies
    llarp::LogInfo("Processing");
    // run system and wait
    n_ctx->Run();
  }
  if(localMode)
  {
    // FIXME: update llarp_main_getLocalRC
    // llarp::RouterContact *rc = llarp_main_getLocalRC(ctx);
    // displayRC(rc);
    // delete it
    if(rc.Read(rcfname))
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
  // it's a unique_ptr, should clean up itself
  // llarp_main_free(ctx);
  return 0;  // success
}
