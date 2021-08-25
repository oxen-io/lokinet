///  Lokinet network extension swift glue layer

import NetworkExtension

enum LokinetError: Error {
    case runtimeError(String)
}

func packet_writer(af: Int32, data: UnsafeRawPointer?, size: Int, ctx: UnsafeMutableRawPointer?) {
    if ctx == nil || data == nil {
        return
    }
    let tunnel = ctx!.assumingMemoryBound(to: PacketTunnelProvider.self).pointee

    let packet = NEPacket(
        data: Data(bytes: data!, count: 1),
        protocolFamily: sa_family_t(af))
    tunnel.packetFlow.writePacketObjects([packet])
}


func start_packet_reader(ctx: UnsafeMutableRawPointer?) {
    let tunnel = ctx?.assumingMemoryBound(to: PacketTunnelProvider.self).pointee
    tunnel?.readHandler()
}


class PacketTunnelProvider: NEPacketTunnelProvider {

    var lokinet: UnsafeMutableRawPointer?

    func readHandler() {
        packetFlow.readPacketObjects() { (packets: [NEPacket]) in
            if self.lokinet == nil {
                return
            }

            for p in packets {
                p.data.withUnsafeBytes { (buf: UnsafeRawBufferPointer) -> Void in
                    llarp_apple_incoming(self.lokinet, buf.baseAddress!, buf.count)
                }
            }

            self.readHandler()
        }
    }

    override func startTunnel(options: [String : NSObject]?, completionHandler: @escaping (Error?) -> Void) {

        var ip_buf = Array<CChar>(repeating: 0, count: 16)
        var mask_buf = Array<CChar>(repeating: 0, count: 16)
        var dns_buf = Array<CChar>(repeating: 0, count: 16)

        lokinet = llarp_apple_init(NSHomeDirectory(), &ip_buf, &mask_buf, &dns_buf)
        if lokinet == nil {
            NSLog("Lokinet initialization failed!")
            completionHandler(LokinetError.runtimeError("Lokinet initialization failed"))
            return
        }

        let ip = String(cString: ip_buf)
        let mask = String(cString: mask_buf)
        let dns_addr = String(cString: dns_buf)

        NSLog("Lokinet configured with address %s/%s, dns %s", ip, mask, dns_addr)

        let settings = NEPacketTunnelNetworkSettings(tunnelRemoteAddress: "127.0.0.1")
        settings.ipv4Settings = NEIPv4Settings(addresses: [ip], subnetMasks: [mask])
        let dns = NEDNSSettings(servers: [dns_addr])
        dns.domainName = "localhost.loki"
        dns.matchDomains = ["snode", "loki"]
        dns.matchDomainsNoSearch = true
        dns.searchDomains = []
        settings.dnsSettings = dns

        let condition = NSCondition()
        condition.lock()
        defer { condition.unlock() }

        var system_error: Error?

        setTunnelNetworkSettings(settings) { error in
            system_error = error
            condition.signal()
        }

        condition.wait()

        if let error = system_error {
            NSLog("Failed to set up tunnel: %s", error.localizedDescription)
            lokinet = nil
            completionHandler(error)
            return
        }

        var myself = self
        let start_ret = llarp_apple_start(lokinet, packet_writer, start_packet_reader, &myself)
        if start_ret != 0 {
            NSLog("Lokinet failed to start!")
            lokinet = nil
            completionHandler(LokinetError.runtimeError("Lokinet failed to start"))
            return
        }
        completionHandler(nil)
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        if lokinet != nil {
            llarp_apple_shutdown(lokinet)
            lokinet = nil
        }
        completionHandler()
    }

    override func handleAppMessage(_ messageData: Data, completionHandler: ((Data?) -> Void)?) {
        if let handler = completionHandler {
            handler(messageData)
        }
    }
    
    override func sleep(completionHandler: @escaping () -> Void) {
        // FIXME - do we need to kick lokinet here?
        completionHandler()
    }
    
    override func wake() {
        // FIXME - do we need to kick lokinet here?
    }
}
