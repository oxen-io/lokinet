import Foundation
import NetworkExtension

/// The general idea here is to intercept packets, pass them to the Lokinet core so that it can modify them as described by the LLARP Traffic Routing Protocol, and then pass them to the appropriate snode(s).
final class LKPacketTunnelProvider : NEPacketTunnelProvider {
    private var daemonContext: LKDaemon.Context?

    override func startTunnel(options: [String:NSObject]? = nil, completionHandler: @escaping (Error?) -> Void) {
        let directoryPath = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.com.niels-andriesse.loki-network")!.path
        let configurationFileName = "lokinet-configuration.ini"
        let bootstrapFileURL = URL(string: "https://i2p.rocks/i2procks.signed")!
        let bootstrapFileName = "bootstrap.signed"
        let daemonConfiguration = LKDaemon.Configuration(isDebuggingEnabled: false, directoryPath: directoryPath, configurationFileName: configurationFileName, bootstrapFileURL: bootstrapFileURL, bootstrapFileName: bootstrapFileName)
        LKUpdateConnectionProgress(0.2)
        LKDaemon.configure(with: daemonConfiguration).done { [weak self] _, context in
            let isSuccess = LKDaemon.start(with: context)
            if isSuccess {
                self?.daemonContext = context
                completionHandler(nil)
            } else {
                let error = LKError.startingDaemonFailed
                LKLog(error.message)
                completionHandler(error)
            }
        }.catch { error in
            if let error = error as? LKError {
                LKLog(error.message)
            } else {
                LKLog("An error occurred.")
            }
            completionHandler(error)
        }
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        if let context = daemonContext { LKDaemon.stop(with: context) }
        completionHandler()
    }
}
