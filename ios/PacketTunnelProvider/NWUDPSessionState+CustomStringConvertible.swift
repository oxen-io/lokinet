import NetworkExtension

extension NWUDPSessionState : CustomStringConvertible {
    
    public var description: String {
        switch self {
        case .invalid: return "invalid"
        case .waiting: return "waiting"
        case .preparing: return "preparing"
        case .ready: return "ready"
        case .failed: return "failed"
        case .cancelled: return "cancelled"
        default: return "unknown"
        }
    }
}
