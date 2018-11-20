#ifndef LLARP_DEFAULTS_HPP
#define LLARP_DEFAULTS_HPP

#ifndef DEFAULT_RESOLVER_US
#define DEFAULT_RESOLVER_US "1.1.1.1"
#endif
#ifndef DEFAULT_RESOLVER_EU
#define DEFAULT_RESOLVER_EU "85.208.208.141"
#endif
#ifndef DEFAULT_RESOLVER_AU
#define DEFAULT_RESOLVER_AU "103.236.162.119"
#endif

#ifdef DEBIAN
#ifndef DEFAULT_LOKINET_USER
#define DEFAULT_LOKINET_USER "debian-lokinet"
#endif
#ifndef DEFAULT_LOKINET_GROUP
#define DEFAULT_LOKINET_GROUP "debian-lokinet"
#endif
#else
#ifndef DEFAULT_LOKINET_USER
#define DEFAULT_LOKINET_USER "lokinet"
#endif
#ifndef DEFAULT_LOKINET_GROUP
#define DEFAULT_LOKINET_GROUP "lokinet"
#endif
#endif

#endif
