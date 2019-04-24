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
    func configure(with configuration: Configuration, completionHandler: @escaping (Result<Void, Error>) -> Void) {
        NETunnelProviderManager.loadAllFromPreferences { [weak self] managers, error in
            if let error = error {
                return completionHandler(.failure(error))
            } else {
                func connect(using manager: NETunnelProviderManager) {
                    self?.manager = manager
                    do {
                        try manager.connection.startVPNTunnel()
                        return completionHandler(.success(()))
                    } catch let error {
                        return completionHandler(.failure(error))
                    }
                }
                if let manager = managers?.first {
                    connect(using: manager)
                } else {
                    let manager = NETunnelProviderManager()
                    let p = NETunnelProviderProtocol()
                    p.providerBundleIdentifier = Bundle.main.bundleIdentifier!
                    p.providerConfiguration = [
                        "port" : configuration.port,
                        "server" : configuration.server,
                        "subnet" : configuration.subnet,
                        "dns" : configuration.dns
                    ]
                    p.serverAddress = configuration.server
                    p.disconnectOnSleep = false
                    manager.protocolConfiguration = p
                    manager.saveToPreferences { error in
                        if let error = error {
                            return completionHandler(.failure(error))
                        } else {
                            manager.loadFromPreferences { error in
                                if let error = error {
                                    return completionHandler(.failure(error))
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
