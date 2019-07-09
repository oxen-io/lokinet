import NetworkExtension
import PromiseKit

/// A UDP based tunnel used for snode communication.
final class LKTunnel : NSObject {
    private let provider: NEPacketTunnelProvider
    private let configuration: Configuration
    private var session: NWUDPSession?
    private var openSeal: PromiseKit.Resolver<Void>?
    
    // MARK: Initialization
    init(provider: NEPacketTunnelProvider, configuration: Configuration) {
        self.provider = provider
        self.configuration = configuration
    }
    
    // MARK: Connection
    func open() -> Promise<Void> {
        let tuple = Promise<Void>.pending()
        openSeal = tuple.resolver
        let endpoint = NWHostEndpoint(hostname: configuration.address, port: String(configuration.port))
        session = provider.createUDPSession(to: endpoint, from: nil)
        LKLog("[Loki] Tunnel (to \(configuration.address):\(configuration.port) state changed to preparing.")
        session!.addObserver(self, forKeyPath: "state", options: .initial, context: &session)
        session!.setReadHandler(read(_:_:), maxDatagrams: Int.max)
        return tuple.promise
    }
    
    override func observeValue(forKeyPath keyPath: String?, of object: Any?, change: [NSKeyValueChangeKey:Any]?, context: UnsafeMutableRawPointer?) {
        if keyPath == "state" && context?.assumingMemoryBound(to: Optional<NWTCPConnection>.self).pointee == session {
            let state = session!.state
            LKLog("[Loki] Tunnel (to \(configuration.address):\(configuration.port) state changed to \(state).")
            switch state {
            case .ready:
                openSeal!.fulfill(())
                openSeal = nil
            case .failed, .cancelled: close()
            default: break
            }
        } else {
            super.observeValue(forKeyPath: keyPath, of: object, change: change, context: context)
        }
    }
    
    private func read(_ datagrams: [Data]?, _ error: Error?) {
        if let datagrams = datagrams {
            LKLog("[Loki] Read datagrams: \(datagrams).")
        } else if let error = error {
            LKLog("[Loki] Couldn't read datagrams due to error: \(error).")
        } else {
            LKLog("[Loki] Couldn't read datagrams.")
        }
    }
    
    func close() {
        session?.cancel()
        openSeal?.reject(LKError.openingTunnelFailed(destination: "\(configuration.address):\(configuration.port)"))
        openSeal = nil
        session?.removeObserver(self, forKeyPath: "state")
        session = nil
    }
}
