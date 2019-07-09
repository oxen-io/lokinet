import UIKit
import NetworkExtension
import MMWormhole

@UIApplicationMain
final class LKAppDelegate : UIResponder, UIApplicationDelegate {
    private let wormhole = MMWormhole(applicationGroupIdentifier: "group.com.niels-andriesse.loki-project", optionalDirectory: "logs")
    private let tunnelProviderManager = NETunnelProviderManager()
    var window: UIWindow?
    
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey:Any]? = nil) -> Bool {
        wormhole.listenForMessage(withIdentifier: "loki") { message in print(message ?? "nil") }
        // TODO: This is meant as a temporary test setup. Eventually we'll want to let the user switch the VPN on and off.
        tunnelProviderManager.loadFromPreferences { [tunnelProviderManager] error in
            if let error = error { return print("[Loki] Couldn't load tunnel configuration due to error: \(error).") }
            let prtcl = NETunnelProviderProtocol()
            prtcl.serverAddress = ""
            prtcl.providerBundleIdentifier = "com.niels-andriesse.loki-network.packet-tunnel-provider"
            tunnelProviderManager.protocolConfiguration = prtcl
            tunnelProviderManager.isEnabled = true
            tunnelProviderManager.isOnDemandEnabled = true
            tunnelProviderManager.saveToPreferences { error in
                if let error = error { return print("[Loki] Couldn't save tunnel configuration due to error: \(error).") }
                tunnelProviderManager.loadFromPreferences { error in
                    if let error = error { return print("[Loki] Couldn't load tunnel configuration due to error: \(error).") }
                    do {
                        try tunnelProviderManager.connection.startVPNTunnel()
                    } catch (let error) {
                        print("[Loki] Couldn't open tunnel due to error: \(error).")
                    }
                }
            }
        }
        return true
    }
}
