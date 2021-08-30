#include <Foundation/Foundation.h>
#include <NetworkExtension/NetworkExtension.h>
#include "context_wrapper.h"

NSString* error_domain = @"com.loki-project.lokinet";

@interface LLARPPacketTunnel : NEPacketTunnelProvider
{
  void* lokinet;
}

- (void)startTunnelWithOptions:(NSDictionary<NSString*, NSObject*>*)options
             completionHandler:(void (^)(NSError* error))completionHandler;

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler;

- (void)handleAppMessage:(NSData*)messageData
       completionHandler:(void (^)(NSData* responseData))completionHandler;

- (void)readPackets;

@end

void nslogger(const char* msg) { NSLog(@"%s", msg); }

void packet_writer(int af, const void* data, size_t size, void* ctx) {
  if (ctx == nil || data == nil)
    return;

  NSData* buf = [NSData dataWithBytesNoCopy:(void*)data length:size freeWhenDone:NO];
  LLARPPacketTunnel* t = (__bridge LLARPPacketTunnel*) ctx;
  NEPacket* packet = [[NEPacket alloc] initWithData:buf  protocolFamily: af];
  [t.packetFlow writePacketObjects:@[packet]];
}

void start_packet_reader(void* ctx) {
  if (ctx == nil)
    return;

  LLARPPacketTunnel* t = (__bridge LLARPPacketTunnel*) ctx;
  [t readPackets];
}

@implementation LLARPPacketTunnel

- (void)readPackets
{
  [self.packetFlow readPacketObjectsWithCompletionHandler: ^(NSArray<NEPacket*>* packets) {
    if (lokinet == nil)
      return;
    for (NEPacket* p in packets) {
      llarp_apple_incoming(lokinet, p.data.bytes, p.data.length);
    }
    [self readPackets];
  }];
}

- (void)startTunnelWithOptions:(NSDictionary<NSString*, NSObject*>*)options
             completionHandler:(void (^)(NSError*))completionHandler
{
  char ip_buf[16];
  char mask_buf[16];
  char dns_buf[16];

  NSString* default_bootstrap = [[NSBundle mainBundle] pathForResource:@"bootstrap" ofType:@"signed"];

  lokinet = llarp_apple_init(nslogger, NSHomeDirectory().UTF8String, default_bootstrap.UTF8String, ip_buf, mask_buf, dns_buf);
  if (!lokinet) {
    NSError *init_failure = [NSError errorWithDomain:error_domain code:500 userInfo:@{@"Error": @"Failed to initialize lokinet"}];
    NSLog(@"%@", [init_failure localizedDescription]);
    return completionHandler(init_failure);
  }

  NSString* ip = [[NSString alloc] initWithUTF8String:ip_buf];
  NSString* mask = [[NSString alloc] initWithUTF8String:mask_buf];
  NSString* dnsaddr = [[NSString alloc] initWithUTF8String:dns_buf];
  
  NEPacketTunnelNetworkSettings* settings =
      [[NEPacketTunnelNetworkSettings alloc] initWithTunnelRemoteAddress:@"127.0.0.1"];
  NEDNSSettings* dns = [[NEDNSSettings alloc] initWithServers:@[dnsaddr]];
  dns.domainName = @"localhost.loki";
  // In theory, matchDomains is supposed to be set to DNS suffixes that we resolve.  This seems
  // highly unreliable, though: often it just doesn't work at all (perhaps only if we make ourselves
  // the default route?), and even when it does work, it seems there are secret reasons that some
  // domains (such as instagram.com) still won't work because there's some magic sauce in the OS
  // that Apple engineers don't want to disclose ("This is what I expected, actually. Although I
  // will not comment on what I believe is happening here", from
  // https://developer.apple.com/forums/thread/685410).
  //
  // So the documentation sucks and the feature doesn't appear to work, so as much as it would be
  // nice to capture only .loki and .snode when not in exit mode, we can't, so capture everything
  // and use our default upstream.
  dns.matchDomains = @[@""];
  dns.matchDomainsNoSearch = true;
  dns.searchDomains = @[];
  NEIPv4Settings* ipv4 = [[NEIPv4Settings alloc] initWithAddresses:@[ip]
                                                       subnetMasks:@[mask]];
  ipv4.includedRoutes = @[[[NEIPv4Route alloc] initWithDestinationAddress:ip subnetMask: mask]];
  settings.IPv4Settings = ipv4;
  settings.DNSSettings = dns;
  [self setTunnelNetworkSettings:settings completionHandler:^(NSError* err) {
    if (err) {
      NSLog(@"Failed to configure lokinet tunnel: %@", err);
      return completionHandler(err);
    }
    
    int start_ret = llarp_apple_start(lokinet, packet_writer, start_packet_reader, (__bridge void*) self);
    if (start_ret != 0) {
      NSError *start_failure = [NSError errorWithDomain:error_domain code:start_ret userInfo:@{@"Error": @"Failed to start lokinet"}];
      NSLog(@"%@", start_failure);
      lokinet = nil;
      return completionHandler(start_failure);
    }
    completionHandler(nil);
    }];
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler
{
  if (lokinet) {
    llarp_apple_shutdown(lokinet);
    lokinet = nil;
  }
  completionHandler();
}

- (void)handleAppMessage:(NSData*)messageData
       completionHandler:(void (^)(NSData* responseData))completionHandler
{
  NSData* response = [NSData dataWithBytesNoCopy:"ok" length:3 freeWhenDone:NO];
  completionHandler(response);
}
@end
