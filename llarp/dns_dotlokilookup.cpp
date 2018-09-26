#include <llarp/dns_dotlokilookup.hpp>
#include <llarp/handlers/tun.hpp>

std::string const default_chars =
    "abcdefghijklmnaoqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

#include <random>

std::string
random_string(size_t len = 15, std::string const &allowed_chars = default_chars)
{
  std::mt19937_64 gen{std::random_device()()};

  std::uniform_int_distribution< size_t > dist{0, allowed_chars.length() - 1};

  std::string ret;

  std::generate_n(std::back_inserter(ret), len,
                  [&] { return allowed_chars[dist(gen)]; });
  return ret;
}

struct check_query_simple_request
{
  const struct sockaddr *from;  // source
  dnsd_question_request *request;
};

std::unordered_map< std::string, struct dnsd_query_hook_response * >
    loki_tld_lookup_cache;

void
llarp_dotlokilookup_checkQuery(void *u, uint64_t orig, uint64_t left)
{
  if(left)
    return;
  // struct check_query_request *request = static_cast< struct
  // check_query_request * >(u);
  struct check_query_simple_request *qr =
      static_cast< struct check_query_simple_request * >(u);
  dotLokiLookup *dll = (dotLokiLookup *)qr->request->context->user;
  if(!dll)
  {
    llarp::LogError("DNSd dotLokiLookup is not configured");
    return;
  }

  // we do have result
  // if so send that
  // else
  // if we have a free private ip, send that
  llarp::service::Address addr;
  if(!addr.FromString(qr->request->question.name))
  {
    llarp::LogWarn("Could not base32 decode address: ",
                   qr->request->question.name);
    delete qr;
    return;
  }
  // cache hit
  auto itr = loki_tld_lookup_cache.find(addr.ToString());
  if(itr != loki_tld_lookup_cache.end())
  {
    writesend_dnss_response(itr->second->returnThis, qr->from, qr->request);
    return;
  }

  struct dns_pointer *free_private = dns_iptracker_get_free(dll->ip_tracker);
  if(free_private)
  {
    in_addr ip_address = ((sockaddr_in *)free_private->hostResult)->sin_addr;

    llarp::handlers::TunEndpoint *tunEndpoint =
        (llarp::handlers::TunEndpoint *)dll->user;
    bool mapResult = tunEndpoint->MapAddress(addr, ntohl(ip_address.s_addr));
    /*
    bool mapResult = main_router_mapAddress(
                                            ctx, addr,
    ntohl(ip_address.s_addr));  // maybe ntohl on the s_addr
    */
    if(!mapResult)
    {
      delete qr;
      return;
    }

    // make a dnsd_query_hook_response for the cache
    dnsd_query_hook_response *response = new dnsd_query_hook_response;
    response->dontLookUp               = true;
    response->dontSendResponse         = false;
    response->returnThis               = free_private->hostResult;
    llarp::LogInfo("Saving ", qr->request->question.name);
    loki_tld_lookup_cache[qr->request->question.name] = response;

    // FIXME: flush cache to disk
    // on crash we'll need to bring up all the same IPs we assigned before...
    writesend_dnss_response(free_private->hostResult, qr->from, qr->request);
    delete qr;
    return;
  }
  // else
  llarp::LogInfo("Sending cname to delay");
  writecname_dnss_response(
      random_string(49, "abcdefghijklmnopqrstuvwxyz") + "bob.loki", qr->from,
      qr->request);
  delete qr;
}

dnsd_query_hook_response *
llarp_dotlokilookup_handler(std::string name, const struct sockaddr *from,
                            struct dnsd_question_request *request)
{
  dnsd_query_hook_response *response = new dnsd_query_hook_response;
  // dotLokiLookup *dll                 = (dotLokiLookup
  // *)request->context->user;
  response->dontLookUp       = false;
  response->dontSendResponse = false;
  response->returnThis       = nullptr;
  llarp::LogDebug("Hooked ", name);
  std::string lName = name;
  std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);

  // FIXME: probably should just read the last 5 bytes
  if(lName.find(".loki") != std::string::npos)
  {
    llarp::LogInfo("Detect Loki Lookup for ", lName);
    auto cache_check = loki_tld_lookup_cache.find(lName);
    if(cache_check != loki_tld_lookup_cache.end())
    {
      // was in cache
      llarp::LogInfo("Reused address from LokiLookupCache");
      // FIXME: avoid the allocation if you could
      delete response;
      return cache_check->second;
    }

    // decode string into HS addr
    llarp::service::Address addr;
    if(!addr.FromString(lName))
    {
      llarp::LogWarn("Could not base32 decode address");
      response->dontSendResponse = true;
      return response;
    }
    llarp::LogInfo("Got address ", addr);

    // start path build early (if you're looking it up, you're probably going to
    // use it)
    // main_router_prefetch(ctx, addr);

    // schedule future response
    check_query_simple_request *qr = new check_query_simple_request;
    qr->from                       = from;
    qr->request                    = request;
    // nslookup on osx is about 5 sec before a retry, 2s on linux
    llarp_logic_call_later(request->context->client.logic,
                           {2000, qr, &llarp_dotlokilookup_checkQuery});

    response->dontSendResponse = true;
  }
  return response;
}
