#include <config/config.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace llarp;
using namespace ::testing;

TEST(Config, smoke)
{
  Config config;
  (void)config;
  SUCCEED();
}

TEST(Config, sample_config)
{
  std::string text = R"(
[router]
# number of crypto worker threads
threads=4
# path to store signed RC
contact-file=/home/lokinet/1/self.signed
# path to store transport private key
transport-privkey=/home/lokinet/1/transport.private
# path to store identity signing key
ident-privkey=/home/lokinet/1/identity.private
# encryption key for onion routing
encryption-privkey=/home/lokinet/1/encryption.private

# uncomment following line to set router nickname to 'lokinet'
netid=bunny

[logging]
level=info
# uncomment for logging to file
#type=file
#file=/path/to/logfile
# uncomment for syslog logging
#type=syslog

# admin api (disabled by default)
[api]
enabled=false
#authkey=insertpubkey1here
#authkey=insertpubkey2here
#authkey=insertpubkey3here
bind=127.0.0.1:1190

# system settings for privileges and such
[system]
user=lokinet
group=lokinet
pidfile=/home/lokinet/1/lokinet.pid

# dns provider configuration section
[dns]
# resolver
upstream=1.1.1.1
bind=127.0.1.1:53

# network database settings block
[netdb]
# directory for network database skiplist storage
dir=/home/lokinet/1/netdb

# bootstrap settings
[bootstrap]
# add a bootstrap node's signed identity to the list of nodes we want to bootstrap from
# if we don't have any peers we connect to this router
add-node=/home/lokinet/1/bootstrap.signed

# snapps configuration section
[services]# uncomment next line to enable a snapp
#example-snapp=/home/lokinet/1/snapp-example.ini

[bind]
eth0=5501


[network]
ifname=cluster-1
ifaddr=10.101.0.1/16

)";

  Config config;
  ASSERT_TRUE(config.LoadFromStr(text));

  {
    using kv = NetworkConfig::NetConfig::value_type;

    ASSERT_THAT(config.network.netConfig(),
                UnorderedElementsAre(kv("ifname", "cluster-1"),
                                     kv("ifaddr", "10.101.0.1/16")));
  }

  {
    using kv = LinksConfig::Links::value_type;

    ASSERT_THAT(config.links.inboundLinks(),
                UnorderedElementsAre(kv("eth0", AF_INET, 5501, {})));
  }

  ASSERT_THAT(config.bootstrap.routers,
              ElementsAre("/home/lokinet/1/bootstrap.signed"));
}
