import NetworkExtension

final class PacketTunnelProvider : NEPacketTunnelProvider {
    
    override func startTunnel(options: [String:NSObject]? = nil, completionHandler: @escaping (Error?) -> Void) {
        print("startTunnel(...)")
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        print("stopTunnel(...)")
    }
}
