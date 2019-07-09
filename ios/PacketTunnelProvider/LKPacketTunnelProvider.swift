import Foundation
import NetworkExtension

final class LKPacketTunnelProvider : NEPacketTunnelProvider {
    
    override func startTunnel(options: [String:NSObject]? = nil, completionHandler: @escaping (Error?) -> Void) {
        let directoryPath = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: "group.com.niels-andriesse.loki-project")!.path
        let configurationFileName = "lokinet-configuration.ini"
        let bootstrapFileURL = URL(string: "https://i2p.rocks/i2procks.signed")!
        let bootstrapFileName = "bootstrap.signed"
        let daemonConfiguration = LKDaemon.Configuration(isDebuggingEnabled: false, directoryPath: directoryPath, configurationFileName: configurationFileName, bootstrapFileURL: bootstrapFileURL, bootstrapFileName: bootstrapFileName)
        LKDaemon.configure(with: daemonConfiguration).done { _, context in
            LKDaemon.start(with: context)
            completionHandler(nil)
        }.catch { error in
            completionHandler(error)
        }
    }
    
    override func stopTunnel(with reason: NEProviderStopReason, completionHandler: @escaping () -> Void) {
        LKDaemon.stop()
        completionHandler()
    }
}
