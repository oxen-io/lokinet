import CoreFoundation
import NetworkExtension

final class LKPacketTunnelProvider : NEPacketTunnelProvider {
    private let daemon = LKDaemon()
    private var tcpTunnel: LKTCPTunnel?
    private var udpTunnel: LKUDPTunnel?
    
    override func startTunnel(options: [String:NSObject]? = nil, completionHandler: @escaping (Error?) -> Void) {
        Darwin.sleep(10) // Give the user time to attach the debugger
        let directoryPath = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.com.niels-andriesse.loki-project").path
        let configurationFileName = "lokinet-configuration.ini"
        let bootstrapFileURL = URL(string: "https://i2p.rocks/i2procks.signed")!
        let bootstrapFileName = "bootstrap.signed"
        let daemonConfiguration = LKDaemon.Configuration(isDebuggingEnabled: false, directoryPath: directoryPath, configurationFileName: configurationFileName, bootstrapFileURL: bootstrapFileURL, bootstrapFileName: bootstrapFileName)
        daemon.configure(with: daemonConfiguration) { [weak self, daemon] result in
            switch result {
            case .success(let configurationFilePath, let context):
                do {
                    guard let strongSelf = self else { return }
                    var isTCPTunnelOpen = false
                    var isUDPTunnelOpen = false
                    let tcpTunnelConfiguration = try LKTCPTunnel.Configuration(fromFileAt: configurationFilePath)
                    let udpTunnelConfiguration = try LKUDPTunnel.Configuration(fromFileAt: configurationFilePath)
                    let tcpTunnel = LKTCPTunnel(provider: strongSelf, configuration: tcpTunnelConfiguration)
                    let udpTunnel = LKUDPTunnel(provider: strongSelf, configuration: udpTunnelConfiguration)
                    var openTCPTunnelError: Error?
                    var openUDPTunnelError: Error?
                    tcpTunnel.open { error in
                        isTCPTunnelOpen = true
                        openTCPTunnelError = error
                        guard isUDPTunnelOpen else { return }
                        completionHandler(openUDPTunnelError ?? error)
                    }
                    udpTunnel.open { error in
                        isUDPTunnelOpen = true
                        openUDPTunnelError = error
                        guard isTCPTunnelOpen else { return }
                        completionHandler(openTCPTunnelError ?? error)
                    }
                    daemon.start(with: context)
                } catch let error {
                    Console.log("[Loki] Failed to parse tunnel configuration due to error: \(error).")
                }
            case .failure(let error): Console.log("[Loki] Failed to configure daemon due to error: \(error).")
            }
        }
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        tcpTunnel?.close()
        udpTunnel?.close()
        completionHandler()
    }
}
