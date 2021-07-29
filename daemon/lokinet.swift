import AppKit
import Foundation
import LokinetExtension
import NetworkExtension

let app = NSApplication.shared

class LokinetMain: NSObject, NSApplicationDelegate {
    var vpnManager = NETunnelProviderManager()
    let lokinetComponent = "com.loki-project.lokinet.network-extension"
    var dnsComponent = "com.loki-project.lokinet.dns-proxy"

    func applicationDidFinishLaunching(_: Notification) {
        setupVPNJizz()
    }

    func bail() {
        app.terminate(self)
    }

    func setupDNSJizz() {
        NSLog("setting up dns settings")
        let dns = NEDNSSettingsManager.shared()
        let settings = NEDNSSettings(servers: ["172.16.0.1"])
        dns.dnsSettings = settings
        dns.loadFromPreferences { [self] (error: Error?) -> Void in
            if let error = error {
                NSLog(error.localizedDescription)
                bail()
                return
            }
            dns.saveToPreferences { [self] (error: Error?) -> Void in
                if let error = error {
                    NSLog(error.localizedDescription)
                    bail()
                    return
                }
                NSLog("dns setting set up probably")
            }
        }
    }

    func setupDNSProxyJizz() {
        NSLog("setting up dns proxy")
        let dns = NEDNSProxyManager.shared()
        let provider = NEDNSProxyProviderProtocol()
        provider.providerBundleIdentifier = dnsComponent
        provider.username = "Anonymous"
        provider.serverAddress = "loki.loki"
        provider.includeAllNetworks = true
        provider.enforceRoutes = true
        dns.providerProtocol = provider
        dns.localizedDescription = "lokinet dns"
        dns.loadFromPreferences { [self] (error: Error?) -> Void in
            if let error = error {
                NSLog(error.localizedDescription)
                bail()
                return
            }
            provider.includeAllNetworks = true
            provider.enforceRoutes = true
            dns.isEnabled = true
            dns.saveToPreferences { [self] (error: Error?) -> Void in
                if let error = error {
                    NSLog(error.localizedDescription)
                    bail()
                    return
                }
                self.initDNSObserver()
                NSLog("dns is up probably")
            }
        }
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
            // macos seems to have trouble when this is true, and reports are that this breaks and
            // doesn't do what it says on the tin in the first place.  Needs more testing.
            providerProtocol.includeAllNetworks = false
            self.vpnManager.protocolConfiguration = providerProtocol
            self.vpnManager.isEnabled = true
            // self.vpnManager.isOnDemandEnabled = true
            let rules = NEAppRule()
            rules.matchDomains = ["*.snode", "*.loki"]
            self.vpnManager.appRules = [rules]
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

    func initDNSObserver() {
        NotificationCenter.default.addObserver(forName: NSNotification.Name.NEDNSProxyConfigurationDidChange, object: NEDNSProxyManager.shared(), queue: OperationQueue.main) { _ -> Void in
            let dns = NEDNSProxyManager.shared()
            NSLog("%@", dns)
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
                self.setupDNSJizz()
            }
        }
    }
}

let delegate = LokinetMain()
app.delegate = delegate
app.run()
