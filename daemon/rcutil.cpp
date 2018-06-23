#include <llarp.h>
#include <signal.h>
#include "logger.hpp"

struct llarp_main *ctx = 0;

llarp_main *sllarp = nullptr;

void
handle_signal(int sig)
{
  if(ctx)
    llarp_main_signal(ctx, sig);
}

#ifndef TESTNET
#define TESTNET 0
#endif

#include <getopt.h>
#include <llarp/router_contact.h>
#include <llarp/time.h>
#include <fstream>
#include "fs.hpp"
#include "buffer.hpp"
#include "crypto.hpp"
#include "router.hpp"

bool printNode(struct llarp_nodedb_iter *iter) {
  char ftmp[68] = {0};
  const char *hexname =
    llarp::HexEncode< llarp::PubKey, decltype(ftmp) >(iter->rc->pubkey, ftmp);

  printf("[%zu]=>[%s]\n", iter->index, hexname);
  return false;
}

void HandleDHTLocate(llarp_router_lookup_job *job) {
  llarp::Info("DHT result: ", job->found ? "found" : "not found");
  // save to nodedb?
}

int
main(int argc, char *argv[])
{
  // take -c to set location of daemon.ini
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
        "--generate with a path to a router contact file\n"
        "--update   with a path to a router contact file\n"
        "--list     \n"
        "--import   with a path to a router contact file\n"
        "--export   a hex formatted public key\n"
        "--locate   a hex formatted public key"
        "\n");
    return 0;
  }
  bool genMode = false;
  bool updMode = false;
  bool listMode = false;
  bool importMode = false;
  bool exportMode = false;
  bool locateMode = false;
  int c;
  char *conffname;
  char defaultConfName[] = "daemon.ini";
  conffname              = defaultConfName;
  char *rcfname;
  char defaultRcName[]   = "other.signed";
  rcfname                = defaultRcName;
  bool haveRequiredOptions = false;
  while(1)
  {
    static struct option long_options[] = {
        {"config", required_argument, 0, 'c'},
        {"generate", required_argument, 0, 'g'},
        {"update", required_argument, 0, 'u'},
        {"list", no_argument, 0, 'l'},
        {"import", required_argument, 0, 'i'},
        {"export", required_argument, 0, 'e'},
        {"locate", required_argument, 0, 'q'},
        {0, 0, 0, 0}};
    int option_index = 0;
    c = getopt_long(argc, argv, "cgluieq", long_options, &option_index);
    if(c == -1)
      break;
    switch(c)
    {
      case 0:
        break;
      case 'c':
        conffname = optarg;
        break;
      case 'l':
        haveRequiredOptions = true;
        listMode = true;
        break;
      case 'i':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname = optarg;
        haveRequiredOptions = true;
        importMode = true;
        break;
      case 'e':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname = optarg;
        haveRequiredOptions = true;
        exportMode = true;
        break;
      case 'q':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname = optarg;
        haveRequiredOptions = true;
        locateMode = true;
        break;
      case 'g':
        // printf ("option -g with value `%s'\n", optarg);
        rcfname = optarg;
        haveRequiredOptions = true;
        genMode = true;
        break;
      case 'u':
        // printf ("option -u with value `%s'\n", optarg);
        rcfname = optarg;
        haveRequiredOptions = true;
        updMode = true;
        break;
      default:
        abort();
    }
  }
  if (!haveRequiredOptions) {
    llarp::Error("Parameters dont all have their required parameters.\n");
    return 0;
  }
  printf("parsed options\n");
  if(!genMode && !updMode && !listMode &&!importMode && !exportMode && !locateMode)
  {
    llarp::Error("I don't know what to do, no generate or update parameter\n");
    return 0;
  }

  ctx = llarp_main_init(conffname, !TESTNET);
  if (!ctx) {
    llarp::Error("Cant set up context");
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

    // which is in daemon.ini config: router.encryption-privkey (defaults "encryption.key")
    fs::path encryption_keyfile = "encryption.key";
    llarp::SecretKey encryption;
    llarp_findOrCreateEncryption(&crypt, encryption_keyfile.c_str(), &encryption);
    llarp_rc_set_pubenckey(&tmp, llarp::seckey_topublic(encryption));

    // get identity public sig key
    fs::path ident_keyfile = "identity.key";
    byte_t identity[SECKEYSIZE];
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.c_str(), identity);
    llarp_rc_set_pubsigkey(&tmp, llarp::seckey_topublic(identity));

    // this causes a segfault
    llarp_rc_sign(&crypt, identity, &tmp);
    // set filename
    fs::path our_rc_file = rcfname;
    // write file
    llarp_rc_write(&tmp, our_rc_file.c_str());
    // release memory for tmp lists
    llarp_rc_free(&tmp);
  }
  if(updMode)
  {
    printf("rcutil.cpp - Loading [%s]\n", rcfname);
    llarp_rc *rc = llarp_rc_read(rcfname);

    // set updated timestamp
    rc->last_updated = llarp_time_now_ms();
    // load longterm identity
    llarp_crypto crypt;
    llarp_crypto_libsodium_init(&crypt);
    fs::path ident_keyfile = "identity.key";
    byte_t identity[SECKEYSIZE];
    llarp_findOrCreateIdentity(&crypt, ident_keyfile.c_str(), identity);
    // get identity public key
    uint8_t *pubkey = llarp::seckey_topublic(identity);
    llarp_rc_set_pubsigkey(rc, pubkey);
    llarp_rc_sign(&crypt, identity, rc);

    // set filename
    fs::path our_rc_file_out = "update_debug.rc";
    // write file
    llarp_rc_write(&tmp, our_rc_file_out.c_str());
  }
  if (listMode) {
    llarp_main_loadDatabase(ctx);
    llarp_nodedb_iter iter;
    iter.visit = printNode;
    llarp_main_iterateDatabase(ctx, iter);
  }
  if (importMode) {
    llarp_main_loadDatabase(ctx);
    llarp::Info("Loading ", rcfname);
    llarp_rc *rc = llarp_rc_read(rcfname);
    if (!rc)
    {
      llarp::Error("Can't load RC");
      return 0;
    }
    llarp_main_putDatabase(ctx, rc);
  }
  if (exportMode) {
    llarp_main_loadDatabase(ctx);
    //llarp::Info("Looking for string: ", rcfname);

    llarp::PubKey binaryPK;
    llarp::HexDecode(rcfname, binaryPK.data());
    
    llarp::Info("Looking for binary: ", binaryPK);
    struct llarp_rc *rc = llarp_main_getDatabase(ctx, binaryPK.data());
    if (!rc) {
      llarp::Error("Can't load RC from database");
    }
    std::string filename(rcfname);
    filename.append(".signed");
    llarp::Info("Writing out: ", filename);
    llarp_rc_write(rc, filename.c_str());
  }
  if (locateMode) {
    llarp::Info("Going online");
    llarp_main_setup(ctx);

    llarp::PubKey binaryPK;
    llarp::HexDecode(rcfname, binaryPK.data());

    llarp::Info("Queueing job");
    llarp_router_lookup_job *job = new llarp_router_lookup_job;
    job->found = false;
    job->hook = &HandleDHTLocate;
    memcpy(job->target, binaryPK, PUBKEYSIZE); // set job's target
    llarp_main_queryDHT(ctx, job);

    llarp::Info("Processing");
    // run system and wait
    llarp_main_run(ctx);
  }
  llarp_main_free(ctx);
  return 1; // success
}
