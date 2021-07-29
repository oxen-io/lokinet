#pragma once
#include <Foundation/Foundation.h>
#include <NetworkExtension/NetworkExtension.h>

struct DNSImpl;

@interface DNSProvider : NEDNSProxyProvider
{
  struct DNSImpl* m_Impl;
}
- (void)startProxyWithOptions:(NSDictionary<NSString*, id>*)options
            completionHandler:(void (^)(NSError* error))completionHandler;

- (void)stopProxyWithReason:(NEProviderStopReason)reason
          completionHandler:(void (^)(void))completionHandler;

- (BOOL)handleNewFlow:(NEAppProxyFlow*)flow;

@end
