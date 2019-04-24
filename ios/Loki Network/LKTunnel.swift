import NetworkExtension

final class LKTunnel {
    private var manager: NETunnelProviderManager?
    
    // MARK: Lifecycle
    static let shared = LKTunnel()
    
    private init() {
        NotificationCenter.default.addObserver(self, selector: #selector(handleVPNStatusChanged), name: Notification.Name.NEVPNStatusDidChange, object: nil)
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    // MARK: Configuration
    func configure(completionHandler: @escaping (Error?) -> Void) {
        NETunnelProviderManager.loadAllFromPreferences { [weak self] managers, e0 in
            if let e0 = e0 {
                return completionHandler(e0)
            } else {
                func connect(using manager: NETunnelProviderManager) {
                    self?.manager = manager
                    do {
                        try manager.connection.startVPNTunnel()
                        return completionHandler(nil)
                    } catch let e3 {
                        return completionHandler(e3)
                    }
                }
                if let manager = managers?.first {
                    connect(using: manager)
                } else {
                    let manager = NETunnelProviderManager()
                    let configuration = NETunnelProviderProtocol()
                    configuration.providerBundleIdentifier = ""
                    configuration.providerConfiguration = [
                        "port" : "",
                        "server" : "",
                        "ip" : "",
                        "subnet" : "",
                        "mtu" : "",
                        "dns" : ""
                    ]
                    configuration.serverAddress = ""
                    configuration.disconnectOnSleep = false
                    manager.protocolConfiguration = configuration
                    manager.saveToPreferences { e1 in
                        if let e1 = e1 {
                            return completionHandler(e1)
                        } else {
                            manager.loadFromPreferences { e2 in
                                if let e2 = e2 {
                                    return completionHandler(e2)
                                } else {
                                    connect(using: manager)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // MARK: Updating
    @objc private func handleVPNStatusChanged() {
        guard let status = manager?.connection.status else { return }
        switch status {
        case .connecting: print("Connecting...")
        case .connected: print("Connected")
        case .disconnecting: print("Disconnecting...")
        case .disconnected: print("Disconnected")
        case .invalid: print("Invalid")
        case .reasserting: print("Reasserting...")
        default: break
        }
    }
}
