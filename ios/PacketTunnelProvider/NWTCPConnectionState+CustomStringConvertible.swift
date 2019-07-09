import NetworkExtension

extension NWTCPConnectionState : CustomStringConvertible {
    
    public var description: String {
        switch self {
        case .connecting: return "connecting"
        case .connected: return "connected"
        case .disconnected: return "disconnected"
        case .cancelled: return "cancelled"
        case .invalid: return "invalid"
        case .waiting: return "waiting"
        default: return "unknown"
        }
    }
}
