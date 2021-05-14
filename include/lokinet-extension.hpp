#pragma once
#include <Foundation/Foundation.h>
#include <NetworkExtension/NetworkExtension.h>

struct ContextWrapper;

@interface LLARPPacketTunnel : NEPacketTunnelProvider
{
 @private
  struct ContextWrapper* m_Context;
}
- (void)startTunnelWithOptions:(NSDictionary<NSString*, NSObject*>*)options
             completionHandler:(void (^)(NSError* error))completionHandler;

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler;

@end
