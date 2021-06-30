#pragma once
#include <Foundation/Foundation.h>
#include <NetworkExtension/NetworkExtension.h>

struct ContextWrapper;

@interface LLARPPacketTunnel : NEPacketTunnelProvider
{
  struct ContextWrapper* m_Context;
}

- (void)startTunnelWithOptions:(NSDictionary<NSString*, NSObject*>*)options
             completionHandler:(void (^)(NSError* error))completionHandler;

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler;

- (void)handleAppMessage:(NSData*)messageData
       completionHandler:(void (^)(NSData* responseData))completionHandler;

@end
