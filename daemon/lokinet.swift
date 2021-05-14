// AppDelegateExtension.swift

import Foundation
import LokinetExtension
import NetworkExtension

class LokinetMain: NSObject {
    var vpnManager = NETunnelProviderManager()

    let lokinetComponent = "org.lokinet.NetworkExtension"
    var lokinetAdminTimer: DispatchSourceTimer?

    func runMain() {
        print("Starting up lokinet")
        NETunnelProviderManager.loadAllFromPreferences { (savedManagers: [NETunnelProviderManager]?, error: Error?) in
            if let error = error {
                print(error)
            }

            if let savedManagers = savedManagers {
                for manager in savedManagers {
                    if (manager.protocolConfiguration as? NETunnelProviderProtocol)?.providerBundleIdentifier == self.lokinetComponent {
                        print("Found saved VPN Manager")
                        self.vpnManager = manager
                    }
                }
            }
            let providerProtocol = NETunnelProviderProtocol()
            providerProtocol.serverAddress = "lokinet"
            providerProtocol.providerBundleIdentifier = self.lokinetComponent
            self.vpnManager.protocolConfiguration = providerProtocol
            self.vpnManager.isEnabled = true
            self.vpnManager.saveToPreferences(completionHandler: { error -> Void in
                if error != nil {
                    print("Error saving to preferences")
                } else {
                    print("saved...")
                    self.vpnManager.loadFromPreferences(completionHandler: { error in
                        if error != nil {
                            print("Error loading from preferences")
                        } else {
                            do {
                                print("Trying to start")
                                self.initializeConnectionObserver()
                                try self.vpnManager.connection.startVPNTunnel()
                            } catch let error as NSError {
                                print(error)
                            } catch {
                                print("There was a fatal error")
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
                print("VPN configuration is invalid")
            } else if self.vpnManager.connection.status == .disconnected {
                print("VPN is disconnected.")
            } else if self.vpnManager.connection.status == .connecting {
                print("VPN is connecting...")
            } else if self.vpnManager.connection.status == .reasserting {
                print("VPN is reasserting...")
            } else if self.vpnManager.connection.status == .disconnecting {
                print("VPN is disconnecting...")
            }
        }
    }
}

let lokinet = LokinetMain()
lokinet.runMain()
