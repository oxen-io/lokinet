import NetworkExtension
import PromiseKit

final class LKLokiNetManager {
    private let tunnelProviderManager = NETunnelProviderManager()
    private(set) var status: Status = .disconnected { didSet { onStatusChanged?() } }
    var onStatusChanged: (() -> Void)?
    
    // MARK: Status
    enum Status { case connecting, connected, disconnecting, disconnected }
    
    // MARK: Initialization & Deinitialization
    static let shared = LKLokiNetManager()
    
    private init() {
        NotificationCenter.default.addObserver(self, selector: #selector(updateStatusIfNeeded), name: .NEVPNStatusDidChange, object: nil)
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    // MARK: Connection Management
    @discardableResult
    func start() -> Promise<Void> {
        return Promise<Void> { [tunnelProviderManager] seal in
            tunnelProviderManager.loadFromPreferences { error in
                guard error == nil else { return seal.reject(LKError.loadingTunnelProviderConfigurationFailed) }
                let prtcl = NETunnelProviderProtocol()
                prtcl.serverAddress = "" // Unused, but the API complains if this isn't set
                prtcl.providerBundleIdentifier = "com.niels-andriesse.loki-network.packet-tunnel-provider"
                tunnelProviderManager.protocolConfiguration = prtcl
                tunnelProviderManager.isEnabled = true
                tunnelProviderManager.isOnDemandEnabled = true
                tunnelProviderManager.saveToPreferences { error in
                    guard error == nil else { return seal.reject(LKError.savingTunnelProviderConfigurationFailed) }
                    tunnelProviderManager.loadFromPreferences { error in
                        guard error == nil else { return seal.reject(LKError.loadingTunnelProviderConfigurationFailed) }
                        do {
                            try tunnelProviderManager.connection.startVPNTunnel()
                            seal.fulfill(())
                        } catch {
                            seal.reject(LKError.startingLokiNetFailed)
                        }
                    }
                }
            }
        }
    }
    
    func stop() {
        tunnelProviderManager.connection.stopVPNTunnel()
    }
    
    // MARK: Updating
    @objc private func updateStatusIfNeeded() {
        let status = tunnelProviderManager.connection.status
        switch status {
        case .connecting: self.status = .connecting
        case .connected: self.status = .connected
        case .disconnecting: self.status = .disconnecting
        case .disconnected: self.status = .disconnected
        default: break
        }
    }
}
