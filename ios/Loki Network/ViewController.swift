import UIKit
import NetworkExtension

class ViewController : UIViewController {

    override func viewDidLoad() {
        super.viewDidLoad()
        // Start tunnel
        let tunnelProviderManager = NETunnelProviderManager()
        let prtcl = NETunnelProviderProtocol()
        prtcl.providerBundleIdentifier = Bundle.main.bundleIdentifier!
        prtcl.serverAddress = "172.16.10.1"
        tunnelProviderManager.protocolConfiguration = prtcl
        tunnelProviderManager.saveToPreferences { error in
            if let error = error {
                print(error)
            } else {
                do {
                    try tunnelProviderManager.connection.startVPNTunnel()
                } catch let error {
                    print(error)
                }
            }
        }
        // Start daemon
        let daemon = Daemon.shared
        daemon.configure(isDebuggingEnabled: true) { [weak daemon] context in
            daemon?.run(with: context)
        }
    }
}
