import UIKit
import NetworkExtension

@UIApplicationMain
final class LKAppDelegate : UIResponder, UIApplicationDelegate {
    var window: UIWindow?
    private let tunnelProviderManager = NETunnelProviderManager()
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey:Any]? = nil) -> Bool {
        // TODO: This is meant as a temporary test setup. Eventually we'll want to let the user switch the VPN on and off.
        tunnelProviderManager.loadFromPreferences { [tunnelProviderManager] error in
            guard error == nil else { return print("[Loki] Couldn't load tunnel configuration due to error: \(error).") }
            let prtcl = NETunnelProviderProtocol()
            prtcl.providerBundleIdentifier = "com.niels-andriesse.loki-network.packet-tunnel-provider"
            tunnelProviderManager.protocolConfiguration = prtcl
            tunnelProviderManager.isEnabled = true
            tunnelProviderManager.isOnDemandEnabled = true
            tunnelProviderManager.saveToPreferences { error in
                guard error == nil else { return print("[Loki] Couldn't save tunnel configuration due to error: \(error).") }
                tunnelProviderManager.loadFromPreferences { error in
                    guard error == nil else { return print("[Loki] Couldn't load tunnel configuration due to error: \(error).") }
                    do {
                        try tunnelProviderManager.connection.startVPNTunnel()
                    } catch (let error) {
                        print("[Loki] Couldn't open tunnel due to error: \(error).")
                    }
                }
            }
        }
    }
}
