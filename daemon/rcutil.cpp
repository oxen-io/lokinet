#include <getopt.h>
#include <llarp.h>
#include <signal.h>
#include "logger.hpp"

#include <llarp/router_contact.h>
#include <llarp/time.h>

#include <fstream>
#include "buffer.hpp"
#include "crypto.hpp"
#include "fs.hpp"
#include "llarp/net.hpp"
#include "router.hpp"

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

bool
printNode(struct llarp_nodedb_iter *iter)
{
  char ftmp[68] = {0};
  const char *hexname =
      llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(iter->rc->pubkey, ftmp);

  printf("[%zu]=>[%s]\n", iter->index, hexname);
  return false;
}

bool
aiLister(struct llarp_ai_list_iter *request, struct llarp_ai *addr)
{
  static size_t count = 0;
  count++;
  llarp::Addr a(*addr);
  std::cout << "AddressInfo " << count << ": " << a << std::endl;
  return true;
}

void
displayRC(llarp_rc *rc)
{
  char ftmp[68] = {0};
  const char *hexPubSigKey =
      llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(rc->pubkey, ftmp);
  printf("PubSigKey [%s]\n", hexPubSigKey);

  struct llarp_ai_list_iter iter;
  // iter.user
  iter.visit = &aiLister;
  llarp_ai_list_iterate(rc->addrs, &iter);
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
    displayRC(&job->result);
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
  // printf("has [%d]options\n", argc);
  if(argc < 2)
  {
    printf(
        "please specify: \n"
        "--generate  with a path to a router contact file\n"
        "--update    with a path to a router contact file\n"
        "--list      \n"
        "--import    with a path to a router contact file\n"
        "--export    a hex formatted public key\n"
        "--locate    a hex formatted public key"
        "--localInfo \n"
        "--read      with a path to a router contact file\n"
        "\n");
    return 0;
  }
  bool genMode    = false;
  bool updMode    = false;
  bool listMode   = false;
  bool importMode = false;
  bool exportMode = false;
  bool locateMode = false;
  bool localMode  = false;
  bool readMode   = false;
  int c;
  char *conffname;
  char defaultConfName[] = "daemon.ini";
  conffname              = defaultConfName;
  char *rcfname;
  char defaultRcName[]     = "other.signed";
  rcfname                  = defaultRcName;
  bool haveRequiredOptions = false;
  while(1)
  {
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"logLevel", required_argument, 0, 'o'},
        {"generate", required_argument, 0, 'g'},
        {"update", required_argument, 0, 'u'},
        {"list", no_argument, 0, 'l'},
        {"import", required_argument, 0, 'i'},
        {"export", required_argument, 0, 'e'},
        {"locate", required_argument, 0, 'q'},
        {"localInfo", no_argument, 0, 'n'},
        {"read", required_argument, 0, 'r'},
        {0, 0, 0, 0}};
    int option_index = 0;
    c = getopt_long(argc, argv, "c:o:g:lu:i:e:q:nr:", long_options,
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
      case 'l':
        haveRequiredOptions = true;
        listMode            = true;
        break;
      case 'i':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname             = optarg;
        haveRequiredOptions = true;
        importMode          = true;
        break;
      case 'e':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname             = optarg;
        haveRequiredOptions = true;
        exportMode          = true;
        break;
      case 'q':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname             = optarg;
        haveRequiredOptions = true;
        locateMode          = true;
        break;
      case 'g':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname             = optarg;
        haveRequiredOptions = true;
        genMode             = true;
        break;
      case 'u':
        // printf ("option -u with value `%s'\n", optarg);
        rcfname             = optarg;
        haveRequiredOptions = true;
        updMode             = true;
        break;
      case 'n':
        haveRequiredOptions = true;
        localMode           = true;
        break;
      case 'r':
        rcfname             = optarg;
        haveRequiredOptions = true;
        readMode            = true;
        break;
      default:
        abort();
    }
  }
#undef MIN
  if(!haveRequiredOptions)
  {
    llarp::LogError("Parameters dont all have their required parameters.\n");
    return 0;
  }
  printf("parsed options\n");
  if(!genMode && !updMode && !listMode && !importMode && !exportMode
     && !locateMode && !localMode && !readMode)
  {
    llarp::LogError(
        "I don't know what to do, no generate or update parameter\n");
    return 0;
  }

  ctx = llarp_main_init(conffname, !TESTNET);
  if(!ctx)
  {
    llarp::LogError("Cant set up context");
    return 0;
  }
  signal(SIGINT, handle_signal);

  llarp_rc tmp;
  if(genMode)
  {
    printf("Creating [%s]\n", rcfname);
    // Jeff wanted tmp to be stack created
    // do we still need to zero it out?
    llarp_rc_clear(&tmp);
    // if we zero it out then
    // allocate fresh pointers that the bencoder can expect to be ready
    tmp.addrs = llarp_ai_list_new();
    tmp.exits = llarp_xi_list_new();
    // set updated timestamp
    tmp.last_updated = llarp_time_now_ms();
    // load longterm identity
    llarp_crypto crypt;
    llarp_crypto_libsodium_init(&crypt);

    // which is in daemon.ini config: router.encryption-privkey (defaults
    // "encryption.key")
    fs::path encryption_keyfile = "encryption.key";
    llarp::SecretKey encryption;

    llarp_findOrCreateEncryption(&crypt, encryption_keyfile.string().c_str(),
                                 &encryption);

    llarp_rc_set_pubenckey(&tmp, llarp::seckey_topublic(encryption));

    // get identity public sig key
    fs::path ident_keyfile = "identity.key";
    byte_t identity[SECKEYSIZE];
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.string().c_str(),
                               identity);

    llarp_rc_set_pubsigkey(&tmp, llarp::seckey_topublic(identity));

    // this causes a segfault
    llarp_rc_sign(&crypt, identity, &tmp);
    // set filename
    fs::path our_rc_file = rcfname;
    // write file
    llarp_rc_write(&tmp, our_rc_file.string().c_str());

    // release memory for tmp lists
    llarp_rc_free(&tmp);
  }
  if(updMode)
  {
    printf("rcutil.cpp - Loading [%s]\n", rcfname);
    llarp_rc rc;
    llarp_rc_clear(&rc);
    llarp_rc_read(rcfname, &rc);

    // set updated timestamp
    rc.last_updated = llarp_time_now_ms();
    // load longterm identity
    llarp_crypto crypt;
    llarp_crypto_libsodium_init(&crypt);
    fs::path ident_keyfile = "identity.key";
    byte_t identity[SECKEYSIZE];
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.string().c_str(),
                               identity);
    // get identity public key
    const uint8_t *pubkey = llarp::seckey_topublic(identity);
    llarp_rc_set_pubsigkey(&rc, pubkey);
    llarp_rc_sign(&crypt, identity, &rc);

    // set filename
    fs::path our_rc_file_out = "update_debug.rc";
    // write file
    llarp_rc_write(&tmp, our_rc_file_out.string().c_str());
  }
  if(listMode)
  {
    llarp_main_loadDatabase(ctx);
    llarp_nodedb_iter iter;
    iter.visit = printNode;
    llarp_main_iterateDatabase(ctx, iter);
  }
  if(importMode)
  {
    llarp_main_loadDatabase(ctx);
    llarp::LogInfo("Loading ", rcfname);
    llarp_rc rc;
    llarp_rc_clear(&rc);
    if(!llarp_rc_read(rcfname, &rc))
    {
      llarp::LogError("Can't load RC");
      return 0;
    }
    llarp_main_putDatabase(ctx, &rc);
  }
  if(exportMode)
  {
    llarp_main_loadDatabase(ctx);
    // llarp::LogInfo("Looking for string: ", rcfname);

    llarp::PubKey binaryPK;
    llarp::HexDecode(rcfname, binaryPK.data());

    llarp::LogInfo("Looking for binary: ", binaryPK);
    struct llarp_rc *rc = llarp_main_getDatabase(ctx, binaryPK.data());
    if(!rc)
    {
      llarp::LogError("Can't load RC from database");
    }
    std::string filename(rcfname);
    filename.append(".signed");
    llarp::LogInfo("Writing out: ", filename);
    llarp_rc_write(rc, filename.c_str());
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
    llarp_rc_new(&job->result);
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
  if(localMode)
  {
    llarp_rc *rc = llarp_main_getLocalRC(ctx);
    displayRC(rc);
  }
  if(readMode)
  {
    llarp_rc result;
    llarp_rc_clear(&result);
    llarp_rc_read(rcfname, &result);
    displayRC(&result);
  }
  // it's a unique_ptr, should clean up itself
  // llarp_main_free(ctx);
  return 1;  // success
}
