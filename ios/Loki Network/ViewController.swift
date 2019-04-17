import UIKit
import NetworkExtension

class ViewController : UIViewController {
    
    override func viewDidLoad() {
        super.viewDidLoad()
        // Set up tunnel
        print("Attempting to set up tunnel...")
        print("--------")
        let tunnelProviderManager = NETunnelProviderManager()
        let _protocol = NETunnelProviderProtocol()
        _protocol.providerBundleIdentifier = Bundle.main.bundleIdentifier!
        tunnelProviderManager.protocolConfiguration = _protocol
        tunnelProviderManager.saveToPreferences { error in
            if let error = error {
                print(error)
                print("--------")
            } else {
                do {
                    print("--------")
                    try tunnelProviderManager.connection.startVPNTunnel()
                    print("Attempting to start daemon...")
                    print("--------")
                    // Start daemon
                    let daemon = Daemon.shared
                    daemon.configure(isDebuggingEnabled: true) { [weak daemon] context in
                        daemon?.run(with: context)
                        print("--------")
                    }
                } catch let error {
                    print(error)
                    print("--------")
                }
            }
        }
    }
}
