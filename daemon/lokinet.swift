import AppKit
import Foundation
import LokinetExtension
import NetworkExtension

let app = NSApplication.shared

class LokinetMain: NSObject, NSApplicationDelegate {
    var vpnManager = NETunnelProviderManager()
    let lokinetComponent = "com.loki-project.lokinet.network-extension"

    func applicationDidFinishLaunching(_: Notification) {
        setupVPNJizz()
    }

    func bail() {
        app.terminate(self)
    }

    func setupVPNJizz() {
        NSLog("Starting up lokinet")
        NETunnelProviderManager.loadAllFromPreferences { [self] (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                NSLog(error.localizedDescription)
                bail()
                return
            }

            if let savedManagers = savedManagers {
                for manager in savedManagers {
                    if (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.lokinetComponent {
                        NSLog("%@", manager)
                        NSLog("Found saved VPN Manager")
                        self.vpnManager = manager
                    }
                }
            }
            let providerProtocol = NETunnelProviderProtocol()
            providerProtocol.serverAddress = "loki.loki" // Needs to be set to some non-null dummy value
            providerProtocol.username = "anonymous"
            providerProtocol.providerBundleIdentifier = self.lokinetComponent
            providerProtocol.enforceRoutes = true
            // macos seems to have trouble when this is true, and reports are that this breaks and
            // doesn't do what it says on the tin in the first place.  Needs more testing.
            providerProtocol.includeAllNetworks = false
            self.vpnManager.protocolConfiguration = providerProtocol
            self.vpnManager.isEnabled = true
            // self.vpnManager.isOnDemandEnabled = true
            self.vpnManager.localizedDescription = "lokinet"
            self.vpnManager.saveToPreferences(completionHandler: { error -> Void in
                if error != nil {
                    NSLog("Error saving to preferences")
                    NSLog(error!.localizedDescription)
                    bail()
                } else {
                    self.vpnManager.loadFromPreferences(completionHandler: { error in
                        if error != nil {
                            NSLog("Error loading from preferences")
                            NSLog(error!.localizedDescription)
                            bail()
                        } else {
                            do {
                                NSLog("Trying to start")
                                self.initializeConnectionObserver()
                                try self.vpnManager.connection.startVPNTunnel()
                            } catch let error as NSError {
                                NSLog(error.localizedDescription)
                                bail()
                            } catch {
                                NSLog("There was a fatal error")
                                bail()
                            }
                        }
                    })
                }
            })
        }
    }

    func initializeConnectionObserver() {
        NotificationCenter.default.addObserver(forName: NSNotification.Name.NEVPNStatusDidChange, object: vpnManager.connection, queue: OperationQueue.main) { _ -> Void in
            if self.vpnManager.connection.status == .invalid {
                NSLog("VPN configuration is invalid")
            } else if self.vpnManager.connection.status == .disconnected {
                NSLog("VPN is disconnected.")
            } else if self.vpnManager.connection.status == .connecting {
                NSLog("VPN is connecting...")
            } else if self.vpnManager.connection.status == .reasserting {
                NSLog("VPN is reasserting...")
            } else if self.vpnManager.connection.status == .disconnecting {
                NSLog("VPN is disconnecting...")
            } else if self.vpnManager.connection.status == .connected {
                NSLog("VPN Connected")
            }
        }
    }
}

let delegate = LokinetMain()
app.delegate = delegate
app.run()
