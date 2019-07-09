import Foundation
import NetworkExtension

final class LKPacketTunnelProvider : NEPacketTunnelProvider {
    private var tcpTunnel: LKTCPTunnel?
    private var udpTunnel: LKUDPTunnel?
    
    override func startTunnel(options: [String:NSObject]? = nil, completionHandler: @escaping (Error?) -> Void) {
        Darwin.sleep(10) // Give the user time to attach the debugger
        let directoryPath = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.com.niels-andriesse.loki-project")!.path
        let configurationFileName = "lokinet-configuration.ini"
        let bootstrapFileURL = URL(string: "https://i2p.rocks/i2procks.signed")!
        let bootstrapFileName = "bootstrap.signed"
        let daemonConfiguration = LKDaemon.Configuration(isDebuggingEnabled: false, directoryPath: directoryPath, configurationFileName: configurationFileName, bootstrapFileURL: bootstrapFileURL, bootstrapFileName: bootstrapFileName)
        LKDaemon.configure(with: daemonConfiguration) { [weak self] result in
            switch result {
            case .success(let configurationFilePath, let context):
                do {
                    guard let strongSelf = self else { return completionHandler("Couldn't open tunnel.") }
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
                    LKDaemon.start(with: context)
                } catch let error {
                    LKLog("[Loki] Couldn't parse tunnel configuration due to error: \(error).")
                }
            case .failure(let error): LKLog("[Loki] Couldn't configure daemon due to error: \(error).")
            }
        }
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        LKDaemon.stop()
        tcpTunnel?.close()
        udpTunnel?.close()
        completionHandler()
    }
}
